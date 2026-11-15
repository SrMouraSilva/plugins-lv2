# Hept7 Switch: its own package of the MOD store, with only the bundle hept7-switch.lv2.
# Upload this file at https://builder.mod.audio/buildroot with a MOD unit connected over USB (the builder takes one
# bundle per package, which is why every plugin of this repository has a recipe of its own: store/*.mk).
# Assembled from store/ of https://github.com/SrMouraSilva/plugins-lv2 -- keep the four in step (see tools/store/pin.py).

# The commit of the repository that is built: it must exist on GitHub. Update it with `python3 tools/store/pin.py`.
HEPT7_SWITCH_VERSION = 3ba199a9fe5dd19772dd0c4503f4a037b081a7c6
HEPT7_SWITCH_SITE = https://github.com/SrMouraSilva/plugins-lv2.git
HEPT7_SWITCH_SITE_METHOD = git
HEPT7_SWITCH_BUNDLES = hept7-switch.lv2

define HEPT7_SWITCH_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/plugins build-hept7-switch
endef

# The bundle goes to the target (as make install does for the device) and is also copied next to the recipe, where
# builder.mod.audio looks for it after the build
define HEPT7_SWITCH_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/plugins install-hept7-switch DESTDIR=$(TARGET_DIR)
	mkdir -p $($(PKG)_PKGDIR)
	cp -r $(TARGET_DIR)/usr/local/lib/lv2/hept7-switch.lv2 $($(PKG)_PKGDIR)/
endef

$(eval $(generic-package))
