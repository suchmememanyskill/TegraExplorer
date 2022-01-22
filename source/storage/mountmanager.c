#include "mountmanager.h"
#include "emummc.h"
#include "../tegraexplorer/tconf.h"
#include "../keys/keys.h"
#include <libs/fatfs/ff.h>
#include <bdk.h>
#include "../config.h"

extern hekate_config h_cfg;

void SetKeySlots(){
    if (TConf.keysDumped){
        se_aes_key_set(0, dumpedKeys.bis_key[0], AES_128_KEY_SIZE);
        se_aes_key_set(1, dumpedKeys.bis_key[0] + AES_128_KEY_SIZE, AES_128_KEY_SIZE);
        se_aes_key_set(2, dumpedKeys.bis_key[1], AES_128_KEY_SIZE);
        se_aes_key_set(3, dumpedKeys.bis_key[1] + AES_128_KEY_SIZE, AES_128_KEY_SIZE);
        se_aes_key_set(4, dumpedKeys.bis_key[2], AES_128_KEY_SIZE);
        se_aes_key_set(5, dumpedKeys.bis_key[2] + AES_128_KEY_SIZE, AES_128_KEY_SIZE);

        // Not for bis but whatever
        se_aes_key_set(6, dumpedKeys.header_key + 0x00, 0x10);
        se_aes_key_set(7, dumpedKeys.header_key + 0x10, 0x10);
        se_aes_key_set(8, dumpedKeys.save_mac_key, 0x10);
    }
}

LIST_INIT(curGpt);

void unmountMMCPart(){
    if (TConf.connectedMMCMounted)
        f_unmount("bis:");
    TConf.connectedMMCMounted = 0;
}

void disconnectMMC(){
    unmountMMCPart();

    if (TConf.currentMMCConnected != MMC_CONN_None){
        TConf.currentMMCConnected = MMC_CONN_None;
        emummc_storage_end();
        emmc_gpt_free(&curGpt);
    }
}


int connectMMC(u8 mmcType){
    if (mmcType == TConf.currentMMCConnected)
        return 0;

    disconnectMMC();
    h_cfg.emummc_force_disable = (mmcType == MMC_CONN_EMMC) ? 1 : 0;
    int res = emummc_storage_init_mmc(&emmc_sdmmc);
    if (!res){
        TConf.currentMMCConnected = mmcType;
        emummc_storage_set_mmc_partition(0);
        emmc_gpt_parse(&curGpt);
    }
        
    return res; // deal with the errors later lol
}

ErrCode_t mountMMCPart(const char *partition){
    if (TConf.currentMMCConnected == MMC_CONN_None)
        return newErrCode(TE_ERR_PARTITION_NOT_FOUND);

    unmountMMCPart();

    emummc_storage_set_mmc_partition(0); // why i have to do this twice beats me
        
    emmc_part_t *system_part = emmc_part_find(&curGpt, partition);
    if (!system_part)
        return newErrCode(TE_ERR_PARTITION_NOT_FOUND);
        
    nx_emmc_bis_init(system_part, true, 0);

    int res = 0;
    if ((res = f_mount(&emmc_fs, "bis:", 1)))
        return newErrCode(res);

    TConf.connectedMMCMounted = 1;
    

    return newErrCode(0);
}

link_t *GetCurGPT(){
    if (TConf.currentMMCConnected != MMC_CONN_None)
        return &curGpt;
    return NULL;
}