# Tetr4 Switch: its own package of the MOD store, with only the bundle tetr4-switch.lv2.
# Upload this file at https://builder.mod.audio/buildroot with a MOD unit connected over USB (the builder takes one
# bundle per package, which is why every plugin of this repository has a recipe of its own: store/*.mk).
# Assembled from store/ of https://github.com/SrMouraSilva/plugins-lv2 -- keep the four in step (see tools/store/pin.py).

# The commit of the repository that is built: it must exist on GitHub. Update it with `python3 tools/store/pin.py`.
TETR4_SWITCH_VERSION = f0c6699975c7c715f5bb77820179be3492598025
TETR4_SWITCH_SITE = https://github.com/SrMouraSilva/plugins-lv2.git
TETR4_SWITCH_SITE_METHOD = git
TETR4_SWITCH_BUNDLES = tetr4-switch.lv2

define TETR4_SWITCH_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/plugins build-tetr4-switch
endef

# The bundle goes to the target (as make install does for the device) and is also copied next to the recipe, where
# builder.mod.audio looks for it after the build
define TETR4_SWITCH_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/plugins install-tetr4-switch DESTDIR=$(TARGET_DIR)
	mkdir -p $($(PKG)_PKGDIR)
	cp -r $(TARGET_DIR)/usr/local/lib/lv2/tetr4-switch.lv2 $($(PKG)_PKGDIR)/
endef

$(eval $(generic-package))
