include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk

PKG_NAME:=smartspeaker
PKG_VERSION:=1.0.1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

define Package/smartspeaker
  SECTION:=utils
  CATEGORY:=zsy
  TITLE:=smartspeaker
endef

define Package/smartspeaker/description
	smart speaker
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		TARGET_BOARD="$(TARGET_BOARD_PLATFORM)"
endef

define Package/smartspeaker/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/smartspeaker	$(1)/usr/bin/

	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libauth.so	$(1)/usr/bin/libauth.so
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libduilite.so $(1)/usr/bin/libduilite.so
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libdds.so	$(1)/usr/bin/libdds.so
	$(INSTALL_DATA) $(CONFIG_TOOLCHAIN_ROOT)/lib/libstdc++.so.6	$(1)/usr/bin/libstdc++.so.6
	$(INSTALL_DATA) /home/r16/PET_R16_Tina/out/astar-parrot/staging_dir/target/usr/lib/libasound.so.2	$(1)/usr/bin/libasound.so.2	
	$(INSTALL_DATA) /home/r16/PET_R16_Tina/out/astar-parrot/staging_dir/target/usr/lib/libmosquitto.so.1	$(1)/usr/bin/libmosquitto.so.1
	$(INSTALL_DATA) /home/r16/PET_R16_Tina/out/astar-parrot/staging_dir/target/usr/lib/libcares.so.2	$(1)/usr/bin/libcares.so.2
	$(INSTALL_DATA) /home/r16/PET_R16_Tina/out/astar-parrot/staging_dir/target/usr/lib/libcrypto.so.1.0.0	$(1)/usr/bin/libcrypto.so.1.0.0
	$(INSTALL_DATA) /home/r16/PET_R16_Tina/out/astar-parrot/staging_dir/target/usr/lib/libssl.so.1.0.0	$(1)/usr/bin/libssl.so.1.0.0

endef

$(eval $(call BuildPackage,smartspeaker))
