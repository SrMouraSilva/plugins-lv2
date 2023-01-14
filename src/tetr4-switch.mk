TETR4_SWITCH_VERSION = b1fa1349
#1.0.0
TETR4_SWITCH_SITE = https://github.com/SrMouraSilva/tetr4-switch-lv2
TETR4_SWITCH_SITE_METHOD = git
# TETR4_SWITCH_SOURCE = calf-$(CALF_LABS_VERSION).tar.gz
# TETR4_SWITCH_DEPENDENCIES = cairo expat libglib2
# TETR4_SWITCH_AUTORECONF = YES
# TETR4_SWITCH_MAKE = $(MAKE1)
TETR4_SWITCH_DEPENDENCIES = juce6
TETR4_SWITCH_CONF_ENV = DESTDIR="$(TARGET_DIR)"
TETR4_SWITCH_CONF_OPTS = -DCMAKE_TOOLCHAIN_FILE=$(JUCE6_TOOLCHAIN_FILE) -DSKIP_LV2_TTL_GENERATOR=ON
TETR4_SWITCH_BUNDLES = tetr4-switch.lv2



#include /home/builder/mod-plugin-builder/plugins/package/tetr4-switch/Makefile

#$(eval $(cmake-package))
$(eval $(generic-package))
#$(eval $(autotools-package))