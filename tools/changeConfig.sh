#!/bin/bash
if [ "$#" != "1" ]; then
  echo "usage: changeConfig.sh <config_path>"
  exit -1
fi
configFile=$1
if [ "$OBSOLETE_KEEP_ADB_SECURE" != "1" ] || \
   [ "$EUCLID_BUILD_INSECURE_KERNEL" == "1" ] || \
   [ "$TARGET_BUILD_VARIANT" != "user" ] ; then \
  sed -i '/CONFIG_SECURITY_SELINUX_DEVELOP.*/d' $configFile
  echo -e "CONFIG_SECURITY_SELINUX_DEVELOP=y\n" >> $configFile
else
  sed -i '/CONFIG_SECURITY_SELINUX_DEVELOP.*/d' $configFile
fi
echo  "===OBSOLETE_KEEP_ADB_SECURE = $OBSOLETE_KEEP_ADB_SECURE"
echo  "===EUCLID_BUILD_INSECURE_KERNEL = $EUCLID_BUILD_INSECURE_KERNEL"
echo  "===TARGET_BUILD_VARIANT = $TARGET_BUILD_VARIANT"


function setting_kernel_config()
{
	config=$1
	value=$2
	string=$3

	sed -i "/${config}.*/d" $configFile

	if [ $string -eq 0 ]
	then
		echo -e "${config}=${value}" >> $configFile
	else
		echo -e "${config}=\"${value}\"" >> $configFile
	fi
}

function delete_kernel_config()
{
        echo  "configFile=== $configFile "
        config=$1
        sed -i "/${config}.*/d" $configFile
}


if [[ "$OPPO_AGING_TYPE" == "KASAN" ]]
then
        #kasan
        setting_kernel_config CONFIG_RANDOMIZE_BASE n 0
        setting_kernel_config CONFIG_SLUB_DEBUG y 0
        setting_kernel_config CONFIG_SLUB_DEBUG_ON y 0
        setting_kernel_config CONFIG_KASAN y 0
        setting_kernel_config CONFIG_KASAN_INLINE y 0
        setting_kernel_config CONFIG_FRAME_WARN 0 0

elif [[ "$OPPO_AGING_TYPE" == "KMEMLEAK" ]]
then
        #kmemleak
        setting_kernel_config CONFIG_RANDOMIZE_BASE n 0
        setting_kernel_config CONFIG_DEBUG_KMEMLEAK y 0
        setting_kernel_config CONFIG_DEBUG_KMEMLEAK_EARLY_LOG_SIZE 9600 0
        setting_kernel_config CONFIG_DEBUG_KMEMLEAK_DEFAULT_OFF n 0
        setting_kernel_config CONFIG_SLUB_DEBUG y 0
        setting_kernel_config CONFIG_SLUB_DEBUG_ON y 0
fi


echo  "===OPPO_AGING_TEST = $OPPO_AGING_TEST ;OPPO_AGING_TYPE = $OPPO_AGING_TYPE"
