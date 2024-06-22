# Pedal Pi - Plugins

Lv2 Utilitary plugins focused on [Mod Dwarf](https://mod.audio/dwarf/) devices.

## Gossiper

Shows a lv2-hmi notification when a button is turned on or turned off. Useful if you use an external midi controller that doesn't have feedback resources like LED. As an educated gossiper, it will only gossipes if you are looking to it (i.e., an "assignable" parameter is showing on device' screen).

## Tetr4 Switch

LV2 audio plugin inspired on switches like Pedrone Penta Switch, Pedrone Yet, Carl Martin Octa Switch and others, but focused on the MOD Devices universe.

With it, is possible to define four presets of active CV and change between then. 

![Web gui sample](./docs/sample.gif)

### TODO

#### v1.0.0

* [ ] Publish

#### v1.1.0

* [ ] Custom preset names - https://pedalboards.moddevices.com/plugins/aHR0cDovL29wZW4tbXVzaWMta29udHJvbGxlcnMuY2gvbHYyL25vdGVzI25vdGVz
* [ ] Custom HMI - https://github.com/moddevices/plugin-examples/tree/main/hmi-widgets-individual-tests
   * [ ] Led colour
   * [ ] Custom label
     * Atom sample 1: https://github.com/moddevices/mod-cv-plugins/blob/master/source/mod-cv-clock/mod-cv-clock.c
     * Atom sample 2: https://lv2plug.in/book/#_state_map_h

## Development

Requirements
```bash
sudo apt install lv2-dev
```

Compile
```bash
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
docker run --rm -ti --name mpb -p 9000:9000 -v $(pwd)/plugins:/root/mod-plugin-builder/plugins/package/pedal-pi-plugins cbix/mod-plugin-builder:moddwarf
sudo apt install iputils-ping --yes
./build moddwarf pedal-pi-plugins
./build moddwarf pedal-pi-plugins-publish

# To force a build without changing the version number into pedal-pi-plugins.mk
rm -r /root/mod-workdir/moddwarf/build/pedal-pi-plugins-*

# Copy files from container
# Note: open a new tab
docker cp mpb:/root/mod-workdir/moddwarf/target/usr/local/lib/lv2/tetr4-switch.lv2/ binaries/dwarf
docker cp mpb:/root/mod-workdir/moddwarf/target/usr/local/lib/lv2/gossiper.lv2/ binaries/dwarf
```


About 

* https://wiki.mod.audio/wiki/How_To_Build_and_Deploy_LV2_Plugin_to_MOD_Duo
* https://wiki.mod.audio/wiki/Preparing_the_Bundle
* https://wiki.mod.audio/wiki/Creating_the_GUI
* https://wiki.mod.audio/wiki/LV2#Logging