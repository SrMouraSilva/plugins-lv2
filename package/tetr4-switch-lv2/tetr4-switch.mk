# TETR4_SWITCH_VERSION = b1fa1349
# #1.0.0
# TETR4_SWITCH_SITE = https://github.com/SrMouraSilva/tetr4-switch-lv2
# TETR4_SWITCH_SITE_METHOD = git
# # TETR4_SWITCH_SOURCE = calf-$(CALF_LABS_VERSION).tar.gz
# # TETR4_SWITCH_DEPENDENCIES = cairo expat libglib2
# # TETR4_SWITCH_AUTORECONF = YES
# # TETR4_SWITCH_MAKE = $(MAKE1)
# TETR4_SWITCH_DEPENDENCIES = juce6
# TETR4_SWITCH_CONF_ENV = DESTDIR="$(TARGET_DIR)"
# TETR4_SWITCH_CONF_OPTS = -DCMAKE_TOOLCHAIN_FILE=$(JUCE6_TOOLCHAIN_FILE) -DSKIP_LV2_TTL_GENERATOR=ON
# TETR4_SWITCH_BUNDLES = tetr4-switch.lv2

# $(eval $(generic-package))




######################################
#
# tetr4-switch
#
######################################

# where to find the source code - locally in this case
TETR4_SWITCH_SITE_METHOD = local
TETR4_SWITCH_SITE = $($(PKG)_PKGDIR)/

# even though this is a local build, we still need a version number
# bump this number if you need to force a rebuild
TETR4_SWITCH_VERSION = 1

# dependencies (list of other buildroot packages, separated by space)
TETR4_SWITCH_DEPENDENCIES =

# LV2 bundles that this package generates (space separated list)
TETR4_SWITCH_BUNDLES = eg-amp.lv2

# call make with the current arguments and path. "$(@D)" is the build directory.
#TETR4_SWITCH_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/source
TETR4_SWITCH_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/src/tetr4-switch.lv2/


# build command
define TETR4_SWITCH_BUILD_CMDS
	$(TETR4_SWITCH_TARGET_MAKE)
endef

# install command
define TETR4_SWITCH_INSTALL_TARGET_CMDS
	$(TETR4_SWITCH_TARGET_MAKE) install DESTDIR=$(TARGET_DIR)
endef


# import everything else from the buildroot generic package
$(eval $(generic-package))
