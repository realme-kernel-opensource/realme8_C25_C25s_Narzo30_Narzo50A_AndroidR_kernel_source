# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2018-2020 Oplus. All rights reserved.
#Kun.Hu@TECH.BSP.Stability.PHOENIX_PROJECT 2019/06/15, Add for PHOENIX_PROJECT
ifeq ($(CONFIG_OPLUS_FEATURE_PHOENIX),y)
ifneq ($(filter oppo6765_20361 oppo6765_20271 oppo6765_206A6  oppo6765_20701, $(OPPO_TARGET_DEVICE)),)
CFLAGS_KERNEL += -DOPPO_MT6765_PLATFORM
CFLAGS_MODULE += -DOPPO_MT6765_PLATFORM
endif

obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += op_bootprof.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_base.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_dump.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_watchdog.o
endif
