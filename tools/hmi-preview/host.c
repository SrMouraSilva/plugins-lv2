/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A tiny LV2 host for the display widgets: it loads the real plugin (`.so`/`.dylib`), plays the part of a device that
 * implements the aLabs HMI widgets extension (it keeps one 1 bit framebuffer for each widget shown) and writes what the
 * plugin drew. `preview.py` drives it and makes the pictures. It is not a model of mod-host: it gives the features the plugins
 * ask for (urid map, the change request, the widget draw feature) and runs `run()` in blocks of 128 frames at 48 kHz.
 *
 * Commands, one per line on the standard input (`#` starts a comment):
 *
 *   control <port> <value>      a control port (input or output), connected with this value
 *   audio_in <port> | audio_out <port>   an audio port; the input carries a 220 Hz sine, an output is discarded
 *   atom_in <port> | atom_out <port>     an atom port, empty on every cycle
 *   set <port> <value>          change the value of a connected control port
 *   label <n> <text>            a patch:Set of the label of preset n (1-based), as MOD UI sends it
 *   addressed <port> | unaddressed <port>   the host tells the plugin a port is (no longer) addressed to a control of the device
 *   activate
 *   show <id> <symbol> <w> <h>  widget_shown: the widget `symbol` on a fresh area of w x h pixels, called `id`
 *   hide <id>                   widget_hidden
 *   run <seconds>
 *   dump <id> <file.pbm>        the framebuffer of the area, a P4 bitmap (a lit pixel is black, as in PBM)
 *   record <prefix> <seconds> <fps> <id>...   run, and every 1/fps s write <prefix><id>_<n>.pbm of each id
 *   hash                        a checksum of everything the audio and CV outputs wrote so far, on the standard output
 *   draws                       how many calls of draw_bitmap since the last time (and the bytes), on the standard error
 *
 * Build: cc host.c -o hmi-host -ldl -I/opt/homebrew/include (see preview.py, which builds it when it is older than this file).
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/patch/patch.h>
#include <lv2/urid/urid.h>

#include "../../plugins/src/hmi/alabs-hmi-widgets.h"
#include "../../plugins/src/lv2/extension/lv2-hmi.h"
#include "../../plugins/src/lv2/extension/control-input-port-change-request.h"

#define MAX_PORTS 256
#define MAX_URIDS 256
#define MAX_AREAS 16
#define BLOCK 128
#define RATE 48000.0
#define ATOM_CAPACITY 16384

typedef struct {
    int      used;
    char     id[32];
    uint16_t width, height;
    uint8_t* bits;
    int      stride;
} Area;

static Area areas[MAX_AREAS];
static unsigned long draw_calls;
static unsigned long draw_bytes;

static char* urids[MAX_URIDS];
static int   urid_count;

static LV2_URID map_uri(LV2_URID_Map_Handle handle, const char* uri) {
    for (int i = 0; i < urid_count; ++i) {
        if (strcmp(urids[i], uri) == 0) {
            return (LV2_URID) (i + 1);
        }
    }
    urids[urid_count] = strdup(uri);
    return (LV2_URID) (++urid_count);
}

static const char* unmap_uri(LV2_URID_Unmap_Handle handle, LV2_URID urid) {
    return urid >= 1 && urid <= (LV2_URID) urid_count ? urids[urid - 1] : NULL;
}

static float* control_ports[MAX_PORTS];
static LV2_Atom_Sequence* atom_in_ports[MAX_PORTS];
static LV2_Atom_Sequence* atom_out_ports[MAX_PORTS];

static LV2_ControlInputPort_Change_Status request_change(LV2_ControlInputPort_Change_Request_Handle handle, uint32_t index, float value) {
    if (index >= MAX_PORTS || control_ports[index] == NULL) {
        return LV2_CONTROL_INPUT_PORT_CHANGE_ERR_INVALID_INDEX;
    }
    // mod-host writes the port at once
    *control_ports[index] = value;
    return LV2_CONTROL_INPUT_PORT_CHANGE_SUCCESS;
}

static Area* find_area(const char* id) {
    for (int i = 0; i < MAX_AREAS; ++i) {
        if (areas[i].used && strcmp(areas[i].id, id) == 0) {
            return &areas[i];
        }
    }
    return NULL;
}

static void draw_bitmap(ALABS_HMI_WidgetDraw_Handle handle, ALABS_HMI_Widget widget, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height, const uint8_t* bits) {
    Area* area = (Area*) widget;
    const int source_stride = (width + 7) / 8;

    ++draw_calls;
    draw_bytes += bits ? (unsigned long) source_stride * height : 0;

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            const int px = x + i;
            const int py = y + j;

            if (px >= area->width || py >= area->height) {
                continue;
            }

            const int on = bits != NULL && (bits[j * source_stride + i / 8] & (0x80 >> (i % 8))) != 0;
            uint8_t* byte = &area->bits[py * area->stride + px / 8];

            if (on) {
                *byte |= (uint8_t) (0x80 >> (px % 8));
            } else {
                *byte &= (uint8_t) ~(0x80 >> (px % 8));
            }
        }
    }
}

static void write_pbm(const Area* area, const char* path) {
    FILE* file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }

    fprintf(file, "P4\n%d %d\n", area->width, area->height);
    fwrite(area->bits, 1, (size_t) area->stride * area->height, file);
    fclose(file);
}

static const LV2_Descriptor* descriptor;
static LV2_Handle instance;
static LV2_URID_Map urid_map = { NULL, map_uri };
static LV2_URID_Unmap urid_unmap = { NULL, unmap_uri };
static LV2_ControlInputPort_Change_Request change_request = { NULL, request_change };
static ALABS_HMI_WidgetDraw widget_draw = { sizeof(ALABS_HMI_WidgetDraw), NULL, draw_bitmap };
static const ALABS_HMI_WidgetNotification* notification;
static float block_in[BLOCK];
/** The outputs (audio and CV), one buffer each, so a refactor can be compared: `hash` prints a checksum of all they wrote */
static float* output_buffers[MAX_PORTS];
static int    output_count;
static uint64_t output_hash = 1469598103934665603ull;
static unsigned long frame_count;

static void reset_atoms(void) {
    for (int i = 0; i < MAX_PORTS; ++i) {
        if (atom_in_ports[i] && atom_in_ports[i]->atom.size == 0) {
            atom_in_ports[i]->atom.type = map_uri(NULL, LV2_ATOM__Sequence);
            atom_in_ports[i]->atom.size = sizeof(LV2_Atom_Sequence_Body);
        }
        if (atom_out_ports[i]) {
            // As a host does: the capacity of the buffer in the size, and an empty sequence head the plugin writes over
            atom_out_ports[i]->atom.type = map_uri(NULL, LV2_ATOM__Chunk);
            atom_out_ports[i]->atom.size = ATOM_CAPACITY;
        }
    }
}

static void run_blocks(double seconds) {
    const unsigned long blocks = (unsigned long) (seconds * RATE / BLOCK + 0.5);

    for (unsigned long b = 0; b < blocks; ++b) {
        for (int i = 0; i < BLOCK; ++i) {
            block_in[i] = 0.5f * (float) __builtin_sin(2.0 * 3.14159265358979 * 220.0 * (double) (frame_count + (unsigned long) i) / RATE);
        }
        reset_atoms();
        descriptor->run(instance, BLOCK);
        frame_count += BLOCK;

        for (int k = 0; k < output_count; ++k) {
            const uint32_t* words = (const uint32_t*) output_buffers[k];
            for (int i = 0; i < BLOCK; ++i) {
                output_hash = (output_hash ^ words[i]) * 1099511628211ull;
            }
        }

        // The label patch:Set is sent once
        for (int i = 0; i < MAX_PORTS; ++i) {
            if (atom_in_ports[i]) {
                atom_in_ports[i]->atom.type = map_uri(NULL, LV2_ATOM__Sequence);
                atom_in_ports[i]->atom.size = sizeof(LV2_Atom_Sequence_Body);
            }
        }
    }
}

static void send_label(unsigned int n, const char* text) {
    for (int i = 0; i < MAX_PORTS; ++i) {
        if (atom_in_ports[i] == NULL) {
            continue;
        }

        LV2_Atom_Forge forge;
        LV2_Atom_Forge_Frame sequence_frame, object_frame;
        char property_uri[256];

        lv2_atom_forge_init(&forge, &urid_map);
        lv2_atom_forge_set_buffer(&forge, (uint8_t*) atom_in_ports[i], ATOM_CAPACITY);
        lv2_atom_forge_sequence_head(&forge, &sequence_frame, 0);
        lv2_atom_forge_frame_time(&forge, 0);
        lv2_atom_forge_object(&forge, &object_frame, 0, map_uri(NULL, LV2_PATCH__Set));
        lv2_atom_forge_key(&forge, map_uri(NULL, LV2_PATCH__property));

        snprintf(property_uri, sizeof(property_uri), "%s#preset_label_%u", descriptor->URI, n);
        lv2_atom_forge_urid(&forge, map_uri(NULL, property_uri));
        lv2_atom_forge_key(&forge, map_uri(NULL, LV2_PATCH__value));
        lv2_atom_forge_string(&forge, text, (uint32_t) strlen(text));
        lv2_atom_forge_pop(&forge, &object_frame);
        lv2_atom_forge_pop(&forge, &sequence_frame);
        return;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <plugin.so> <plugin uri>   (commands on the standard input)\n", argv[0]);
        return 2;
    }

    void* library = dlopen(argv[1], RTLD_NOW);
    if (library == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }

    LV2_Descriptor_Function get = (LV2_Descriptor_Function) dlsym(library, "lv2_descriptor");
    for (uint32_t i = 0; get != NULL && (descriptor = get(i)) != NULL; ++i) {
        if (strcmp(descriptor->URI, argv[2]) == 0) {
            break;
        }
    }
    if (descriptor == NULL) {
        fprintf(stderr, "no plugin %s in %s\n", argv[2], argv[1]);
        return 1;
    }

    const LV2_Feature f_map = { LV2_URID__map, &urid_map };
    const LV2_Feature f_unmap = { LV2_URID__unmap, &urid_unmap };
    const LV2_Feature f_change = { LV2_CONTROL_INPUT_PORT_CHANGE_REQUEST_URI, &change_request };
    const LV2_Feature f_draw = { ALABS_HMI__WidgetDraw, &widget_draw };
    const LV2_Feature* features[] = { &f_map, &f_unmap, &f_change, getenv("HMI_NO_WIDGETS") ? NULL : &f_draw, NULL };

    instance = descriptor->instantiate(descriptor, RATE, ".", features);
    if (instance == NULL) {
        fprintf(stderr, "instantiate failed\n");
        return 1;
    }
    notification = (const ALABS_HMI_WidgetNotification*) descriptor->extension_data(ALABS_HMI__WidgetNotification);

    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        char* hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char command[32] = "";
        unsigned int port = 0;
        float value = 0.0f;
        char name[256] = "";
        char symbol[64] = "";
        char path[256] = "";
        int w = 0, h = 0;
        double seconds = 0.0, fps = 0.0;

        if (sscanf(line, "%31s", command) != 1) {
            continue;
        }

        if (strcmp(command, "control") == 0 && sscanf(line, "%*s %u %f", &port, &value) == 2 && port < MAX_PORTS) {
            control_ports[port] = (float*) calloc(1, sizeof(float));
            *control_ports[port] = value;
            descriptor->connect_port(instance, port, control_ports[port]);
        } else if (strcmp(command, "audio_in") == 0 && sscanf(line, "%*s %u", &port) == 1 && port < MAX_PORTS) {
            descriptor->connect_port(instance, port, block_in);
        } else if (strcmp(command, "audio_out") == 0 && sscanf(line, "%*s %u", &port) == 1 && port < MAX_PORTS) {
            output_buffers[output_count] = (float*) calloc(BLOCK, sizeof(float));
            descriptor->connect_port(instance, port, output_buffers[output_count++]);
        } else if (strcmp(command, "atom_in") == 0 && sscanf(line, "%*s %u", &port) == 1 && port < MAX_PORTS) {
            atom_in_ports[port] = (LV2_Atom_Sequence*) calloc(1, ATOM_CAPACITY);
            descriptor->connect_port(instance, port, atom_in_ports[port]);
        } else if (strcmp(command, "atom_out") == 0 && sscanf(line, "%*s %u", &port) == 1 && port < MAX_PORTS) {
            atom_out_ports[port] = (LV2_Atom_Sequence*) calloc(1, ATOM_CAPACITY);
            descriptor->connect_port(instance, port, atom_out_ports[port]);
        } else if (strcmp(command, "set") == 0 && sscanf(line, "%*s %u %f", &port, &value) == 2 && port < MAX_PORTS && control_ports[port]) {
            *control_ports[port] = value;
        } else if (strcmp(command, "label") == 0 && sscanf(line, "%*s %u %255[^\n]", &port, name) == 2) {
            // One block each: the host's buffer has room for one message a cycle
            send_label(port, name);
            run_blocks((double) BLOCK / RATE);
        } else if ((strcmp(command, "addressed") == 0 || strcmp(command, "unaddressed") == 0) && sscanf(line, "%*s %u", &port) == 1) {
            const LV2_HMI_PluginNotification* hmi = (const LV2_HMI_PluginNotification*) descriptor->extension_data(LV2_HMI__PluginNotification);
            static const LV2_HMI_AddressingInfo info = { 0, 0, "", 0.0f, 1.0f, 33 };

            if (hmi == NULL) {
                fprintf(stderr, "the plugin has no HMI notification\n");
                return 1;
            }
            if (command[0] == 'a') {
                hmi->addressed(instance, port, (LV2_HMI_Addressing) (uintptr_t) 1, &info);
            } else {
                hmi->unaddressed(instance, port);
            }
        } else if (strcmp(command, "activate") == 0) {
            if (descriptor->activate) descriptor->activate(instance);
        } else if (strcmp(command, "show") == 0 && sscanf(line, "%*s %31s %63s %d %d", name, symbol, &w, &h) == 4) {
            Area* area = NULL;
            for (int i = 0; i < MAX_AREAS && area == NULL; ++i) {
                if (!areas[i].used) area = &areas[i];
            }
            if (area == NULL || notification == NULL) {
                fprintf(stderr, "cannot show %s\n", name);
                return 1;
            }
            area->used = 1;
            snprintf(area->id, sizeof(area->id), "%s", name);
            area->width = (uint16_t) w;
            area->height = (uint16_t) h;
            area->stride = (w + 7) / 8;
            area->bits = (uint8_t*) calloc(1, (size_t) area->stride * (size_t) h);

            ALABS_HMI_WidgetArea geometry = { sizeof(ALABS_HMI_WidgetArea), (uint16_t) w, (uint16_t) h, ALABS_HMI_PixelFormat_Mono1 };
            notification->widget_shown(instance, symbol, area, &geometry);
        } else if (strcmp(command, "hide") == 0 && sscanf(line, "%*s %31s", name) == 1) {
            Area* area = find_area(name);
            if (area && notification) {
                notification->widget_hidden(instance, area);
                area->used = 0;
                free(area->bits);
            }
        } else if (strcmp(command, "run") == 0 && sscanf(line, "%*s %lf", &seconds) == 1) {
            run_blocks(seconds);
        } else if (strcmp(command, "dump") == 0 && sscanf(line, "%*s %31s %255s", name, path) == 2) {
            Area* area = find_area(name);
            if (area == NULL) {
                fprintf(stderr, "no area %s\n", name);
                return 1;
            }
            write_pbm(area, path);
        } else if (strcmp(command, "record") == 0) {
            char* rest = line;
            char prefix[256];
            int consumed = 0;

            if (sscanf(rest, "%*s %255s %lf %lf%n", prefix, &seconds, &fps, &consumed) < 3) {
                fprintf(stderr, "bad record\n");
                return 1;
            }
            rest += consumed;

            char ids[8][32];
            int id_count = 0;
            while (id_count < 8 && sscanf(rest, "%31s%n", ids[id_count], &consumed) == 1) {
                rest += consumed;
                ++id_count;
            }

            const int frames = (int) (seconds * fps + 0.5);
            for (int f = 0; f < frames; ++f) {
                run_blocks(1.0 / fps);
                for (int k = 0; k < id_count; ++k) {
                    Area* area = find_area(ids[k]);
                    if (area == NULL) {
                        fprintf(stderr, "no area %s\n", ids[k]);
                        return 1;
                    }
                    snprintf(path, sizeof(path), "%s%s_%04d.pbm", prefix, ids[k], f);
                    write_pbm(area, path);
                }
            }
        } else if (strcmp(command, "hash") == 0) {
            printf("hash %016llx\n", (unsigned long long) output_hash);
        } else if (strcmp(command, "draws") == 0) {
            fprintf(stderr, "draws %lu bytes %lu\n", draw_calls, draw_bytes);
            draw_calls = 0;
            draw_bytes = 0;
        } else {
            fprintf(stderr, "unknown command: %s", line);
            return 1;
        }
    }

    descriptor->cleanup(instance);
    return 0;
}
