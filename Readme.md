# SrMouraSilva - Plugins

Lv2 Utilitary plugins focused on [Mod Dwarf](https://mod.audio/dwarf/) devices.

## Gossiper

Shows a lv2-hmi notification when a button is turned on or turned off. Useful if you use an external midi controller that doesn't have feedback resources like LED. As an educated gossiper, it will only gossipes if you are looking to it (i.e., an "assignable" parameter is showing on device' screen).

* 7 footswitches (each mirrored on a `CV Foot - Out N` output) and 4 potentiometers (scaled into a `[Minimum, Maximum]` CV range)
* **Custom footswitch labels**: click on a footswitch label in the plugin GUI and type a name (up to 15 characters). The notification shows this name, and it is saved with the pedalboard/preset
* Ports organised in groups (footswitches, notifications, each potentiometer, CV outputs), so they are easier to find on the device's assignment screen
* Requires MOD Dwarf firmware v1.13 or newer for the notifications; on older firmware, or on devices without HMI such as the MOD Duo, the plugin still loads and the CV outputs still work (just without notifications)

[User manual](./plugins/resources/gossiper.lv2/docs/manual.md) ([PDF](./plugins/resources/gossiper.lv2/modgui/manual.pdf)) · [Forum thread](https://forum.mod.audio/t/gossiper-plugin-to-say-what-is-happening-when-you-are-looking-it/9660)

## Tetr4 Switch

LV2 audio plugin inspired on loop/preset switchers for guitar rigs, but focused on the MOD Devices universe.

With it, is possible to define four presets of active CV and change between then. 

![Web gui sample](./docs/sample.gif)

* 4 CV outputs, with a dip switch per output in each of the 4 presets, and a footswitch-assignable selector per preset
* **Custom preset labels**: each of the four presets can be renamed (instead of `Prst 1`...`Prst 4`) from the plugin GUI. The labels are saved with the pedalboard/preset and are sent back to the GUI after a state restore
* **Custom HMI**: the preset footswitches and the preset selector knob get their own LED colour (preset 1 red, 2 yellow, 3 cyan, 4 magenta; lit while the preset is active, off otherwise), and the label of the active preset is shown as a notification on the device screen (like Gossiper, it needs a page with one of the notify parameters assigned). The HMI `WidgetControl` feature is optional, so the plugin also loads on hosts without it, such as the MOD Duo
* Ports organised in groups (preset selection, one group per preset)
* 4 factory presets: *Four-scene guitar rig*, *One-of-four selector*, *Arrangement builder* and *Rhythm / Lead / Ambient / Kill*
* [User manual (PDF)](./plugins/resources/tetr4-switch.lv2/modgui/manual.pdf) · [Forum thread](https://forum.mod.audio/t/tetr4-switch-plugin-in-beta-store/8998)

### TODO

#### v1.0.0

* [ ] Publish

#### v1.1.0

* [x] Custom preset names - https://pedalboards.moddevices.com/plugins/aHR0cDovL29wZW4tbXVzaWMta29udHJvbGxlcnMuY2gvbHYyL25vdGVzI25vdGVz
* [x] Custom HMI - https://github.com/moddevices/plugin-examples/tree/main/hmi-widgets-individual-tests
   * [x] Led colour
   * [x] Custom label
     * Atom sample 1: https://github.com/moddevices/mod-cv-plugins/blob/master/source/mod-cv-clock/mod-cv-clock.c
     * Atom sample 2: https://lv2plug.in/book/#_state_map_h

## Download

Binaries for **MOD Duo**, **MOD Duo X** and **MOD Dwarf** are built by [GitHub Actions](./.github/workflows/release.yml) on every push and published on the [Releases](../../releases) page. Releases have no version number; they are named after the commit they were built from:

```
<commit date UTC>-<branch>-<commit hash>
20261113-1530-master-87a3e32
```

Each release has one archive per plugin and device (`gossiper-duo.lv2.tar.gz`, `tetr4-switch-duo-x.lv2.tar.gz`, `gossiper-dwarf.lv2.tar.gz`, ...); each one contains the `.lv2` bundle folder.

The release marked **Latest** is the most recent build of `master`. Builds from other branches are published as pre-releases.

## Development

Requirements
```bash
sudo apt install lv2-dev
```

Compile
```bash
cd plugins
make build
```

Install
```bash
make install-user
# uninstalling
#make uninstall-user
```

Alternatively, compile on docker environment
```bash
docker build . -t builder
```

## Build

```bash
docker run --rm -ti --name mpb -p 9000:9000 -v $(pwd)/plugins:/root/mod-plugin-builder/plugins/package/srmourasilva-plugins cbix/mod-plugin-builder:moddwarf
#sudo apt install iputils-ping --yes
# ping alternative
cat > /usr/bin/ping << 'EOF'
#!/bin/bash
host="${@: -1}"
timeout 2 bash -c "echo > /dev/tcp/$host/22" 2>/dev/null && echo "$host is alive" && exit 0
exit 1
EOF
chmod +x /usr/bin/ping

./build moddwarf srmourasilva-plugins
./build moddwarf srmourasilva-plugins-publish

# To force a build without changing the version number into srmourasilva-plugins.mk
rm -r /root/mod-workdir/moddwarf/build/srmourasilva-plugins-*

# Copy files from container (optional, CI already does this - see Releases)
# Note: open a new tab
docker cp mpb:/root/mod-workdir/moddwarf/target/usr/local/lib/lv2/tetr4-switch.lv2/ binaries/dwarf
docker cp mpb:/root/mod-workdir/moddwarf/target/usr/local/lib/lv2/gossiper.lv2/ binaries/dwarf
```

## About 

* https://wiki.mod.audio/wiki/How_To_Build_and_Deploy_LV2_Plugin_to_MOD_Duo
* https://wiki.mod.audio/wiki/Preparing_the_Bundle
* https://wiki.mod.audio/wiki/Creating_the_GUI
* https://wiki.mod.audio/wiki/LV2#Logging

## License

Licensed under the [Apache License, Version 2.0](./LICENSE). See [NOTICE](./NOTICE) for third-party components that keep their original licenses.
