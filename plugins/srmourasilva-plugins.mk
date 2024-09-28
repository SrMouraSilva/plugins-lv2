######################################
#
# gossiper and tetr4-switch
#
######################################

# where to find the source code - locally in this case
SRMOURASILVA_PLUGINS_SITE_METHOD = local
SRMOURASILVA_PLUGINS_SITE = $($(PKG)_PKGDIR)/

# even though this is a local build, we still need a version number
# bump this number if you need to force a rebuild
SRMOURASILVA_PLUGINS_VERSION = 1

# dependencies (list of other buildroot packages, separated by space)
#SRMOURASILVA_PLUGINS_DEPENDENCIES =

# LV2 bundles that this package generates (space separated list)
SRMOURASILVA_PLUGINS_BUNDLES = gossiper.lv2 tetr4-switch.lv2 

# call make with the current arguments and path. "$(@D)" is the build directory.
#SRMOURASILVA_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/source
#SRMOURASILVA_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/src/tetr4-switch.lv2/
SRMOURASILVA_PLUGINS_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/


# build command
define SRMOURASILVA_PLUGINS_BUILD_CMDS
	$(SRMOURASILVA_PLUGINS_TARGET_MAKE)
endef

# install command
define SRMOURASILVA_PLUGINS_INSTALL_TARGET_CMDS
	$(SRMOURASILVA_PLUGINS_TARGET_MAKE) install DESTDIR=$(TARGET_DIR)
endef


# import everything else from the buildroot generic package
$(eval $(generic-package))
