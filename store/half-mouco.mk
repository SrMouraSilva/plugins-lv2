# Half Mouco: its own package of the MOD store, with only the bundle half-mouco.lv2.
# Upload this file at https://builder.mod.audio/buildroot with a MOD unit connected over USB (the builder takes one
# bundle per package, which is why every plugin of this repository has a recipe of its own: store/*.mk).
# Assembled from store/ of https://github.com/SrMouraSilva/plugins-lv2 -- keep the four in step (see tools/store/pin.py).

# The commit of the repository that is built: it must exist on GitHub. Update it with `python3 tools/store/pin.py`.
HALF_MOUCO_VERSION = 110131566a7256b3466352efb2494513927c0e4c
HALF_MOUCO_SITE = https://github.com/SrMouraSilva/plugins-lv2.git
HALF_MOUCO_SITE_METHOD = git
HALF_MOUCO_BUNDLES = half-mouco.lv2

define HALF_MOUCO_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/plugins build-half-mouco
endef

# The bundle goes to the target (as make install does for the device) and is also copied next to the recipe, where
# builder.mod.audio looks for it after the build
define HALF_MOUCO_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/plugins install-half-mouco DESTDIR=$(TARGET_DIR)
	mkdir -p $($(PKG)_PKGDIR)
	cp -r $(TARGET_DIR)/usr/local/lib/lv2/half-mouco.lv2 $($(PKG)_PKGDIR)/
endef

$(eval $(generic-package))
