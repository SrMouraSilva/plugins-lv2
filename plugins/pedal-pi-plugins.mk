# PEDAL_PI_PLUGINS_VERSION = b1fa1349
# #1.0.0
# PEDAL_PI_PLUGINS_SITE = https://github.com/SrMouraSilva/plugins
# PEDAL_PI_PLUGINS_SITE_METHOD = git
# # PEDAL_PI_PLUGINS_SOURCE = calf-$(CALF_LABS_VERSION).tar.gz
# # PEDAL_PI_PLUGINS_DEPENDENCIES = cairo expat libglib2
# # PEDAL_PI_PLUGINS_AUTORECONF = YES
# # PEDAL_PI_PLUGINS_MAKE = $(MAKE1)
# PEDAL_PI_PLUGINS_DEPENDENCIES = juce6
# PEDAL_PI_PLUGINS_CONF_ENV = DESTDIR="$(TARGET_DIR)"
# PEDAL_PI_PLUGINS_CONF_OPTS = -DCMAKE_TOOLCHAIN_FILE=$(JUCE6_TOOLCHAIN_FILE) -DSKIP_LV2_TTL_GENERATOR=ON
# remove underline from: PEDAL_PI_PLUGINS_BUNDLE_S = tetr4-switch.lv2

# $(eval $(generic-package))




######################################
#
# tetr4-switch
#
######################################

# where to find the source code - locally in this case
PEDAL_PI_PLUGINS_SITE_METHOD = local
PEDAL_PI_PLUGINS_SITE = $($(PKG)_PKGDIR)/

# even though this is a local build, we still need a version number
# bump this number if you need to force a rebuild
PEDAL_PI_PLUGINS_VERSION = 1

# dependencies (list of other buildroot packages, separated by space)
PEDAL_PI_PLUGINS_DEPENDENCIES =

# LV2 bundles that this package generates (space separated list)
PEDAL_PI_PLUGINS_BUNDLES = gossiper.lv2 tetr4-switch.lv2 

# call make with the current arguments and path. "$(@D)" is the build directory.
#PEDAL_PI_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/source
#PEDAL_PI_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/src/tetr4-switch.lv2/
PEDAL_PI_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/


# build command
define PEDAL_PI_PLUGINS_BUILD_CMDS
	$(PEDAL_PI_PLUGINS_TARGET_MAKE)
endef

# install command
define PEDAL_PI_PLUGINS_INSTALL_TARGET_CMDS
	$(PEDAL_PI_PLUGINS_TARGET_MAKE) install DESTDIR=$(TARGET_DIR)
endef


# import everything else from the buildroot generic package
$(eval $(generic-package))
