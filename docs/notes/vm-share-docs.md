# VMware shared folder — PDF documentation

Read-only listing of the PDFs in `/mnt/hgfs/vm_share`, the VMware shared folder from
the VM's host PC, as seen from the SDK host `tom-VM` on 2026-10-11.
No PDF was opened beyond `pdfinfo`, and none was copied, moved or committed.
Lab L35 needs the RK3588 TRM.

## Answer for L35

**The RK3588 TRM is present**, in two parts, version V1.0 (2022-03-09):

| File | Pages | pdfinfo CreationDate |
|---|---|---|
| `/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part1-20220309.pdf` | 2287 | Tue Mar 15 19:57:48 2022 CST |
| `/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part2 20220309.pdf` | 3706 | Tue Mar 15 21:40:24 2022 CST |

Which chapters (e.g. VOP2, MIPI DSI) are in which Part is **unknown**: this survey
only uses file names and `pdfinfo`.

**RK3588 datasheets:** `Rockchip RK3588 Datasheet V1.1-20220124.pdf` and
`Rockchip_RK3588_Datasheet_V1.7-20231117.pdf`. The V1.7 file's embedded Title is
"Rockchip RK3588 Datasheet V1.5", so the file name and metadata disagree.
There are also datasheets for RK3588J, RK3588M, RK3588S and RK3588S2.

**LubanCat-5IO schematic / hardware manual: not found.** The only board schematic
whose name matches is `LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf` (LubanCat
**5IOB** carrier board). The running board reports `Embedfire LubanCat-5IO`
(see [board-runtime.md](board-runtime.md)); whether the 5IOB schematic applies to
it is **unknown**. The listing also has `LubanCat5CB_EBF410152V1R0_SCH_20240820.pdf`
(5CB core board), which the name filter in step 2 doesn't match, so it got no
`pdfinfo`.

## 1. Listing

```console
$ find /mnt/hgfs/vm_share -iname '*.pdf' -printf '%s\t%p\n' | sort -k2
```

647 PDFs, more than ~150, so here is the full list for `/mnt/hgfs/vm_share/lubancat/` (461 files) and counts for the rest.

Counts per top-level directory:

| Directory | PDFs |
|---|---|
| `Alpha_imx6ull/` | 186 |
| `frdm-imx95/` | 0 |
| `lubancat/` | 461 |

<details><summary>Full list for <code>lubancat/</code> (size in bytes, path)</summary>

```
5390303	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AMP/Rockchip_Developer_Guide_AMP_CN.pdf
380241	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AUDIO/Algorithms/Rockchip_Developer_Guide_Audio_Detection.pdf
5924740	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AUDIO/Algorithms/Rockchip_Developer_Guide_Microphone_Array_Tuning.pdf
387421	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AUDIO/Algorithms/Rockchip_Developer_Guide_Sound_Event_Detection.pdf
1249844	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AUDIO/Rockchip_Developer_Guide_Audio_CN.pdf
378780	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AUDIO/Rockchip_Developer_Guide_PulseAudio_CN.pdf
255734	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RKeMMCSupportList_Ver1.68_20220815.pdf
281389	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RKeMMCSupportList_Ver1.74_20230411.pdf
313897	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RKeMMCSupportList_Ver1.86_20241129.pdf
300629	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RKNandFlashSupportList_Ver2.73_20180615.pdf
140054	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RK_SpiNor_and_SLC_Nand_SupportList_V1.41_20230303.pdf
271594	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RK_SpiNor_and_SLC_Nand_SupportList_V1.51_20241101.pdf
127422	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RK_SpiNor_and_SLC_Nand_SupportList_Ver1.37_20220513.pdf
141075	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/RK_UFS_SupportList_V1.01_20241012.pdf
1687487	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/Rockchip_Support_List_DDR_Ver2.53.pdf
412727	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/Rockchip_Support_List_DDR_Ver2.56.pdf
180662	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/Rockchip_Support_List_Linux_WiFi_BT_20220605.pdf
132236	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/Rockchip_Support_List_Linux_WiFi_BT_20220828.pdf
289990	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/AVL/Rockchip_Support_List_Linux_WiFi_BT_Ver1.9_20240329.pdf
2380089	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Camera_Module_OTP_Calibration_Guide/Rockchip Camera Module OTP Calibration Guide_v1.0.24.pdf
7459628	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Color_Optimization_Guide_ISP/CN/Rockchip_Color_Optimization_Guide_ISP21_CN_v2.0.1.pdf
4636976	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Development_Guide_ISP/CN/Rockchip_Development_Guide_ISP21_CN_v2.1.1.pdf
2626339	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Driver_Guide_VI/CN/Rockchip_Driver_Guide_VI_CN_v1.1.3.pdf
1522298	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Driver_Guide_VI/EN/Rockchip_Driver_Guide_VI_EN_v1.0.7.pdf
23378167	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_IQ_Tools_Guide_ISP/CN/Rockchip_IQ_Tools_Guide_v2.0.6_CN.pdf
91022106	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP21/Rockchip_Tuning_Guide_ISP/CN/Rockchip_Tuning_Guide_ISP21_CN_v2.1.1.pdf
9501761	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Color_Optimization_Guide_ISP20_CN_v1.2.8.pdf
7459628	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Color_Optimization_Guide_ISP21_CN_v2.0.1.pdf
9769480	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Color_Optimization_Guide_ISP2x_CN_v1.2.5.pdf
7198332	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Color_Optimization_Guide_ISP2x_EN_v1.2.1.pdf
6993863	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Development_Guide_ISP20_CN_v1.7.2.pdf
4688148	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Development_Guide_ISP20_EN_v1.6.4.pdf
4341295	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Development_Guide_ISP21_CN_v2.1.0.pdf
3664438	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Driver_Guide_VI_CN_v1.0.8.pdf
1623739	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Driver_Guide_VI_EN_v1.0.8.pdf
8787173	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_IQ_Tools_Guide_ISP2x_CN_v2.0.3.pdf
11519124	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_IQ_Tools_Guide_ISP2x_EN_v1.2.2.pdf
37164171	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Tuning_Guide_ISP20_CN_v1.6.6.pdf
37354480	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Tuning_Guide_ISP20_CN_v1.6.7.pdf
38691812	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Tuning_Guide_ISP20_EN_v1.6.2.pdf
90947666	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP2X/Rockchip_Tuning_Guide_ISP21_CN_v2.1.0.pdf
1743503	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Camera_Module_OTP_Calibration_Guide/CN/Rockchip Camera Module OTP Calibration Guide_CN_v1.0.34.pdf
2090947	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Camera_Module_OTP_Calibration_Guide/EN/Rockchip Camera Module OTP Calibration Guide_EN_v1.0.34.pdf
14280480	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Color_Optimization_Guide_ISP30/CN/Rockchip_Color_Optimization_Guide_ISP30_CN_v3.0.2.pdf
13166699	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Color_Optimization_Guide_ISP30/EN/Rockchip_Color_Optimization_Guide_ISP30_EN_v3.0.2.pdf
1080438	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Development_Guide_3A_ISP30/CN/Rockchip_Development_Guide_3A_ISP30_CN_v1.1.0.pdf
376438	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Development_Guide_3A_ISP30/EN/Rockchip_Development_Guide_3A_ISP30_EN_v1.1.0.pdf
7511724	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Development_Guide_ISP30/CN/Rockchip_Development_Guide_ISP30_CN_v1.2.5.pdf
2607172	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Development_Guide_ISP30/EN/Rockchip_Development_Guide_ISP30_EN_v1.2.5.pdf
4562900	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Driver_Guide_VI_ISP30/CN/Rockchip_Driver_Guide_VI_CN_v1.1.5.pdf
2108597	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Driver_Guide_VI_ISP30/EN/Rockchip_Driver_Guide_VI_EN_v1.1.5.pdf
93163074	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Tuning_Guide_ISP30/CN/Rockchip_Tuning_Guide_ISP30_CN_v1.2.0.pdf
91171959	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP30/Rockchip_Tuning_Guide_ISP30/EN/Rockchip_Tuning_Guide_ISP30_EN_v1.2.0.pdf
12426739	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP32-lite/Rockchip_Color_Optimization_Guide_ISP/Rockchip_Color_Optimization_Guide_ISP32_Lite_CN_v3.1.0.pdf
3605282	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP32-lite/Rockchip_Development_Guide_ISP/Rockchip_Development_Guide_ISP32_Lite_CN_v1.0.0.pdf
2661858	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP32-lite/Rockchip_Driver_Guide_VI/CN/Rockchip_Driver_Guide_VI_CN_v1.1.4.pdf
23412243	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP32-lite/Rockchip_IQ_Tools_Guide_ISP/Rockchip_IQ_Tools_Guide_v2.0.7_CN.pdf
91398647	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP32-lite/Rockchip_Tuning_Guide_ISP/CN/Rockchip_Tuning_Guide_ISP32-lite_CN_v1.0.0.pdf
22942058	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Color_Optimization_Guide/CN/Rockchip_Color_Optimization_Guide_ISP39_ISP33_CN_v1.2.0.pdf
29827555	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Color_Optimization_Guide/EN/Rockchip_Color_Optimization_Guide_ISP39_ISP33_EN_v1.2.0.pdf
1548531	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Development_Guide_3A_ISP/CN/Rockchip_Development_Guide_3A_ISP39.pdf
20902862	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Development_Guide_ISP/CN/Rockchip_Development_Guide_ISP39_CN_alpha.pdf
15070225	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_IQ_Tools_Guide/CN/Rockchip_IQ_Tools_Guide_ISP33_ISP39_CN_v3.0.2.pdf
19093049	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_IQ_Tools_Guide/EN/Rockchip_IQ_Tools_Guide_ISP33_ISP39_EN_v3.0.2.pdf
38921435	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Tuning_Guide_ISP/CN/Rockchip_Tuning_Guide_ISP39_CN_v2.0.pdf
52606782	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP39/Rockchip_Tuning_Guide_ISP/EN/Rockchip_Tuning_Guide_ISP39_EN_v2.0.pdf
13231205	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_Color_Optimization_Guide_ISP30_CN_v3.0.0.pdf
1080438	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_Development_Guide_3A_ISP30_v1.1.0.pdf
7339829	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_Development_Guide_ISP30_CN_v1.2.3.pdf
2268915	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_Driver_Guide_VI_CN_v1.1.1.pdf
17305397	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_IQ_Tools_Guide_ISP21_ISP30_CN_v2.0.4.pdf
93086859	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAMERA/ISP3X/Rockchip_Tuning_Guide_ISP30_CN_v1.1.0.pdf
170970	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAN/Rockchip_Developer_Guide_Can_CN.pdf
181559	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAN/Rockchip_Developer_Guide_CAN_FD_CN.pdf
275187	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAN/Rockchip_Develop_Guide_Can_CN.pdf
271623	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CAN/Rockchip_Develop_Guide_CAN_FD_CN.pdf
673179	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Developer_Guide_Clock_CN.pdf
217491	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Developer_Guide_Gpio_Output_Clocks_CN.pdf
919308	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Developer_Guide_Linux4.4_4.19_Clock_CN.pdf
590577	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Developer_Guide_Pll_Ssmod_Clock_CN.pdf
435619	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Develop_Guide_Gpio_Output_Clocks_CN.pdf
674241	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CLK/Rockchip_Develop_Guide_Pll_Ssmod_Clock_CN.pdf
747683	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/CRYPTO/Rockchip_Developer_Guide_Crypto_HWRNG_CN.pdf
723089	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Developer-Guide-DDR-CN.pdf
2603790	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Developer_Guide_DDR_CN.pdf
311082	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Developer-Guide-DDR-EN.pdf
1152669	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Developer-Guide-DDR-Problem-Solution-CN.pdf
1027384	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Developer-Guide-DDR-Problem-Solution-EN.pdf
607441	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Developer-Guide-DDR-Verification-Process-CN.pdf
308383	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Developer_Guide_DDR_Verification_Process_EN.pdf
355776	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Developer_Guide_HAL_DDR_ECC_CN.pdf
491553	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-Guide-DDR-PCB-Layout-Notes-CN.pdf
348237	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Introduction_DDR_Bandwidth_Tool_CN.pdf
1343916	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Trouble_Shooting_DDR_CN.pdf
1033313	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip_Trouble_Shooting_DDR_EN.pdf
553222	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DDR/Rockchip-User-Guide-DDR-DQ-Eye-Tool-CN.pdf
5336210	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_Developer_Guide_DS5_CN.pdf
1759791	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_Developer_Guide_FT232H_USB2JTAG.pdf
600745	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_Developer_Guide_GDB_Over_ADB_CN.pdf
7510898	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_Developer_Guide_GNU_MCU_Eclipse_OpenOCD_CN.pdf
2236993	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_Developer_Guide_OpenOCD_CN.pdf
2540507	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DEBUG/Rockchip_User_Guide_J-Link_CN.pdf
435390	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/BT656-BT1120/Rockchip_BT656_TX_AND_BT1120_TX_Developer_Guide_CN.pdf
1535976	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_Developer_Guide_DisplayPort_CN.pdf
174939	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_DisplayPort_SI_Test_Guide_CN.pdf
505255	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3399_Developer_Guide_DisplayPort_CN.pdf
1312266	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf
276814	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DRM/Rockchip_Developer_Guide_DRM_Direct_Show_CN.pdf
2215435	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DRM/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf
3146598	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/eDP/Rockchip_Developer_Guide_eDP_CN.pdf
414341	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDCP/Rockchip_RK3588_Developer_Guide_HDCP_CN.pdf
2102162	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf
868073	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI_CN.pdf
577509	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf
1664718	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/LVDS/Rockchip_Developer_Guide_LVDS_CN.pdf
3519712	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_Developer_Guide_MIPI_DSI2_CN.pdf
3063220	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf
1296253	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RGB/Rockchip_Developer_Guide_RGB_MCU_CN.pdf
2388440	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RK628/Rockchip_Developer_Guide_RK628_For_All_Porting_CN.pdf
540016	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RK628/Rockchip_DRM_RK628_Porting_Guide_CN.pdf
428752	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RK628/Rockchip_MCU_RK628D_Porting_Guide_CN.pdf
346835	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RK628/Rockchip_RK628D_Application_Notes_CN.pdf
2084987	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/RK628/Rockchip_RK628D_For_All_Porting_Guide_CN.pdf
656162	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_BT656_TX_AND_BT1120_TX_Developer_Guide_CN.pdf
317136	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_Baseparameter_Format_Define_And_Use_CN.pdf
99345	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_Buildroot_Weston_EN.pdf
2294980	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf
2199243	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf
1720384	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI_CN.pdf
577509	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf
283653	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Develop_Guide_DRM_Direct_Show_CN.pdf
1448568	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_DRM_Display_Driver_Development_Guide_V1.0.pdf
1777893	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_DRM_Panel_Porting_Guide_V1.6_20190228.pdf
540016	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_DRM_RK628_Porting_Guide_CN.pdf
428752	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_MCU_RK628D_Porting_Guide_CN.pdf
1312266	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf
3063220	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf
598989	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf
346835	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK628D_Application_Notes_CN.pdf
2084987	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK628D_For_All_Porting_Guide_CN.pdf
3529374	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_VOP2_Plane_Assign.pdf
1540317	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Vsync/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf
261032	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DSMC/Rockchip_Developer_Guide_DSMC_CN.pdf
948726	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DSMC/Rockchip_Developer_Guide_SLAVE_DSMC_CN.pdf
351751	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DVFS/Rockchip_Developer_Guide_CPUFreq_CN.pdf
268545	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DVFS/Rockchip_Developer_Guide_CPUFreq_EN.pdf
371005	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DVFS/Rockchip_Developer_Guide_Devfreq_CN.pdf
333119	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DVFS/Rockchip_Developer_Guide_Devfreq_EN.pdf
174158	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/FLEXBUS/Rockchip_Developer_Guide_Linux_FLEXBUS_ADC_and_DAC_MODE_CN.pdf
121624	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/FLEXBUS/Rockchip_Developer_Guide_Linux_FLEXBUS_CN.pdf
118695	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/FLEXBUS/Rockchip_Developer_Guide_Linux_FLEXBUS_FSPI_MODE_CN.pdf
1284505	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/FS/Rockchip_Developer_FAQ_FileSystem_CN.pdf
213311	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_CN.pdf
171602	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_DPDK_CN.pdf
991710	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf
754716	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf
689427	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_EN.pdf
211459	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/GMAC/Rockchip_Developer_Guide_Linux_MAC_TO_MAC_CN.pdf
1118728	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_IN_Based_On_CameraHal3_CN.pdf
802128	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_RX_CN.pdf
359274	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/I2C/Rockchip_Developer_Guide_I2C_CN.pdf
315278	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/I2C/Rockchip_Developer_Guide_I2C_EN.pdf
743995	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/IO-DOMAIN/Rockchip_Developer_Guide_Linux_IO_DOMAIN_CN.pdf
230170	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/IOMMU/Rockchip_Developer_Guide_Linux_IOMMU_CN.pdf
1354935	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP1.X/camera_engine_rkisp_user_manual_v2.0.pdf
3906392	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP1.X/RKISP1_IQ_Parameters_User_Guide_v1.0_20190606.pdf
1430736	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP1.X/RKISP_Driver_User_Manual_v1.3_20190919.pdf
2380089	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_Camera_Module_OTP_Calibration_Guide_v1.0.24.pdf
7459628	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_Color_Optimization_Guide_ISP21_CN_v2.0.1.pdf
4636976	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_Development_Guide_ISP21_CN_v2.1.1.pdf
2626339	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_Driver_Guide_VI_CN_v1.1.3.pdf
23378167	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_IQ_Tools_Guide_v2.0.6_CN.pdf
91022106	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP21/Rockchip_Tuning_Guide_ISP21_CN_v2.1.1.pdf
2380089	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Camera_Module_OTP_Calibration_Guide_v1.0.24.pdf
13231205	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Color_Optimization_Guide_ISP30_CN_v3.0.0.pdf
1080438	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Development_Guide_3A_ISP30_v1.1.0.pdf
7511724	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Development_Guide_ISP30_CN_v1.2.5.pdf
2626339	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Driver_Guide_VI_CN_v1.1.3.pdf
23378167	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_IQ_Tools_Guide_v2.0.6_CN.pdf
92723590	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP30/Rockchip_Tuning_Guide_ISP30_CN_v1.2.1.pdf
12426739	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP32-lite/Rockchip_Color_Optimization_Guide_ISP32_Lite_CN_v3.1.0.pdf
3605282	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP32-lite/Rockchip_Development_Guide_ISP32_Lite_CN_v1.0.0.pdf
2661858	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP32-lite/Rockchip_Driver_Guide_VI_CN_v1.1.4.pdf
23410654	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP32-lite/Rockchip_IQ_Tools_Guide_CN_v2.0.7.pdf
91398647	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP32-lite/Rockchip_Tuning_Guide_ISP32-lite_CN_v1.0.0.pdf
22942058	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Color_Optimization_Guide/CN/Rockchip_Color_Optimization_Guide_ISP39_ISP33_CN_v1.2.0.pdf
29827555	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Color_Optimization_Guide/EN/Rockchip_Color_Optimization_Guide_ISP39_ISP33_EN_v1.2.0.pdf
1548531	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Development_Guide_3A_ISP/CN/Rockchip_Development_Guide_3A_ISP39.pdf
20902862	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Development_Guide_ISP/CN/Rockchip_Development_Guide_ISP39_CN_alpha.pdf
15070225	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_IQ_Tools_Guide/CN/Rockchip_IQ_Tools_Guide_ISP33_ISP39_CN_v3.0.2.pdf
19093049	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_IQ_Tools_Guide/EN/Rockchip_IQ_Tools_Guide_ISP33_ISP39_EN_v3.0.2.pdf
38921435	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Tuning_Guide_ISP/CN/Rockchip_Tuning_Guide_ISP39_CN_v2.0.pdf
52606782	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/ISP/ISP39/Rockchip_Tuning_Guide_ISP/EN/Rockchip_Tuning_Guide_ISP39_EN_v2.0.pdf
213061	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MCU/Rockchip_RK3399_Developer_Guide_MCU_CN.pdf
155206	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MCU/Rockchip_RK3399_Developer_Guide_MCU_EN.pdf
220874	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MEMORY/Rockchip_Developer_Guide_Linux_CMA_CN.pdf
130918	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MEMORY/Rockchip_Developer_Guide_Linux_DMABUF_CN.pdf
187350	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MEMORY/Rockchip_Developer_Guide_Linux_Meminfo_CN.pdf
149474	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MEMORY/Rockchip_Developer_Guide_Linux_Memory_Allocator_CN.pdf
541897	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MMC/Rockchip_Developer_Guide_SD_Boot_CN.pdf
1071643	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MMC/Rockchip_Developer_Guide_SDMMC_SDIO_eMMC_CN.pdf
1450375	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MPP/Rockchip_Developer_Guide_MPP_CN.pdf
392804	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/README.pdf
239677	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_Quick_Start_RKNN_Toolkit2_CN-1.3.0.pdf
292188	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_Quick_Start_RKNN_Toolkit2_CN-1.4.0.pdf
251957	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_Quick_Start_RKNN_Toolkit2_EN-1.3.0.pdf
951125	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_User_Guide_RKNN_Toolkit2_CN-1.3.0.pdf
952322	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_User_Guide_RKNN_Toolkit2_CN-1.4.0.pdf
964465	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknn-toolkit2/Rockchip_User_Guide_RKNN_Toolkit2_EN-1.3.0.pdf
58526719	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/01_Rockchip_RKNPU_Quick_Start_RKNN_SDK_V2.3.0_CN.pdf
10668854	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/02_Rockchip_RKNPU_User_Guide_RKNN_SDK_V2.3.0_CN.pdf
723331	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/03_Rockchip_RKNPU_API_Reference_RKNN_Toolkit2_V2.3.0_CN.pdf
987099	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/04_Rockchip_RKNPU_API_Reference_RKNNRT_V2.3.0_CN.pdf
3709706	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/05_RKNN_Compiler_Support_Operator_List_V2.3.0.pdf
534243	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/RKNN_Compiler_Support_Operator_List_v1.4.0.pdf
5179711	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_Quick_Start_RKNN_SDK_V1.3.0_CN.pdf
5179755	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_Quick_Start_RKNN_SDK_V1.4.0_CN.pdf
621293	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_RKNPU_User_Guide_RKNN_API_V1.3.0_CN.pdf
537125	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_RKNPU_User_Guide_RKNN_API_V1.3.0_EN.pdf
648784	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_RKNPU_User_Guide_RKNN_API_V1.4.0_CN.pdf
825997	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_RV1106_Quick_Start_RKNN_SDK_V1.3.0b0_CN.pdf
968230	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NPU/rknpu2/Rockchip_RV1106_Quick_Start_RKNN_SDK_V1.4.0_CN.pdf
3294567	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Application_Notes_Storage_CN.pdf
1731706	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_FAQ_Storage_CN.pdf
514216	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_Dual_Storage_CN.pdf
666095	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_Linux_Flash_Open_Source_Solution_CN.pdf
491918	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_OTP_CN.pdf
183611	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_SATA_CN.pdf
2204484	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_Secure_Boot_Application_Note_EN.pdf
914277	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_Secure_Boot_for_UBoot_Next_Dev_CN.pdf
856202	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_Secure_Boot_for_UBoot_Next_Dev_EN.pdf
300016	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/NVM/Rockchip_Developer_Guide_UFS_CN.pdf
1938794	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PCIe/Rockchip_Developer_Guide_PCIe_CN.pdf
2859987	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PCIe/Rockchip_Developer_Guide_PCIE_EP_Stardard_Card_CN.pdf
415609	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PCIe/Rockchip_Developer_Guide_PCIe_Performance_CN.pdf
143629	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PCIe/Rockchip_PCIe_Virtualization_Developer_Guide_CN.pdf
198540	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PCIe/Rockchip_RK3399_Developer_Guide_PCIe_CN.pdf
164321	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Developer_Guide_Linux_RealTime_Performance_Test_Report_CN.pdf
520452	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Optimize_Tutorial_Linux_IO_CN.pdf
1560238	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Quick_Start_Linux_Perf_CN.pdf
407909	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Quick_Start_Linux_Performance_Analyse_CN.pdf
1869351	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Quick_Start_Linux_Streamline_CN.pdf
2384473	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PERF/Rockchip_Quick_Start_Linux_Systrace_CN.pdf
439129	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/Pin-Ctrl/Rockchip_Developer_Guide_Linux_Pinctrl_CN.pdf
214235	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PINCTRL/Rockchip_Developer_Guide_Linux_Pinctrl_CN.pdf
104185	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/Pin-Ctrl/Rockchip_Developer_Guide_Linux_Pinctrl_EN.pdf
151448	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_Developer_Guide_Power_Discrete_DCDC_EN.pdf
588910	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK805_Developer_Guide_CN.pdf
480978	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK806_Developer_Guide_CN.pdf
809419	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK808_Developer_Guide_CN.pdf
845365	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK809_Developer_Guide_CN.pdf
653059	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK816_Developer_Guide_CN.pdf
868388	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK817_Developer_Guide_CN.pdf
719296	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK818_Developer_Guide_CN.pdf
333266	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK818_RK816_Developer_Guide_Fuel_Gauge_CN.pdf
157179	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PMIC/Rockchip_RK818_RK816_Introduction_Fuel_Gauge_Log_CN.pdf
1348634	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/POWER/Rockchip_Developer_Guide_Power_Analysis_CN.pdf
1257842	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/POWER/Rockchip_Developer_Guide_Power_Analysis_EN.pdf
331130	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PWM/Rockchip_Developer_Guide_Linux_PWM_CN.pdf
264111	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/PWM/Rockchip_Developer_Guide_Linux_PWM_EN.pdf
615094	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RGA/Rockchip_Developer_Guide_RGA_CN.pdf
6010602	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RGA/Rockchip_FAQ_RGA_CN.pdf
177090	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_CAN_CANFD_CN.pdf
700975	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_Display_CN.pdf
133163	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_I2C_CN.pdf
346364	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_SPI2APB_CN.pdf
406773	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_SPI_CN.pdf
645529	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_SPIFLASH_CN.pdf
162348	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_UART.pdf
754543	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_USB_CN.pdf
375803	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_Ymodem_CN.pdf
138908	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SARADC/Rockchip_Developer_Guide_Linux_SARADC_CN.pdf
331057	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SECURITY/Rockchip_Developer_Guide_Anti_Copy_Board_CN.pdf
449203	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SECURITY/Rockchip_Developer_Guide_OTP_CN.pdf
1261172	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SECURITY/Rockchip_Developer_Guide_TEE_SDK_CN.pdf
380947	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SPI/Rockchip_Developer_Guide_Linux_SPI_CN.pdf
158312	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/SPI/Rockchip_Developer_Guide_Linux_SPI_EN.pdf
320767	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/THERMAL/Rockchip_Developer_Guide_Thermal_CN.pdf
222620	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/THERMAL/Rockchip_Developer_Guide_Thermal_EN.pdf
507746	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/Production-Guide-For-Firmware-Download.pdf
356976	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/RKUpgrade_Dll_UserManual.pdf
156535	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/Rockchip_Introduction_Partition_CN.pdf
141051	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/Rockchip_Introduction_Partition_EN.pdf
389933	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/Rockchip_User_Guide_Production_For_Firmware_Download_CN.pdf
667062	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TOOL/Rockchip-User-Guide-ProductionTool-CN.pdf
1290954	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_Developer_Guide_Trust_CN.pdf
1298975	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_Developer_Guide_Trust_EN.pdf
197343	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3308_Developer_Guide_System_Suspend_CN.pdf
145429	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3308_Developer_Guide_System_Suspend_EN.pdf
163565	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3399_Developer_Guide_System_Suspend_CN.pdf
154979	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK356X_Developer_Guide_System_Suspend_CN.pdf
221680	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3576_Developer_Guide_System_Suspend_CN.pdf
204573	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3588_Developer_Guide_System_Suspend_CN.pdf
200672	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UART/Rockchip_Developer_Guide_UART_CN.pdf
143581	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UART/Rockchip_Developer_Guide_UART_EN.pdf
341878	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UART/Rockchip_Developer_Guide_UART_FAQ_CN.pdf
619766	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_Linux_AB_System_CN.pdf
340934	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_UBoot_MMC_Device_Analysis_CN.pdf
390171	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_UBoot_MTD_Block_Device_Design_CN.pdf
3491626	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_UBoot_Nextdev_CN.pdf
538912	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_U-Boot_TFTP_Upgrade_CN.pdf
205908	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Developer_Guide_U-Boot_TFTP_Upgrade_EN.pdf
327825	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/UBOOT/Rockchip_Introduction_UBoot_rkdevelop_vs_nextdev_CN.pdf
187191	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_Linux_USB_Initialization_Log_Analysis_CN.pdf
543952	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_Linux_USB_Performance_Analysis_CN.pdf
1937867	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_Linux_USB_PHY_CN.pdf
144836	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB2_Compliance_Test_CN.pdf
2954758	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB_CN.pdf
2242084	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB_EN.pdf
166836	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB_FFS_Test_Demo_CN.pdf
755645	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf
5330416	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Developer_Guide_USB_SQ_Test_CN.pdf
609071	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Introduction_USB_SQ_Tool_CN.pdf
1160433	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3399_Developer_Guide_USB_CN.pdf
1415020	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3399_Developer_Guide_USB_DTS_CN.pdf
1691750	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf
1841887	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3576_Developer_Guide_USB_CN.pdf
3527224	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3588_Developer_Guide_USB_CN.pdf
512260	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Trouble_Shooting_Linux4.19_USB_Gadget_UVC_CN.pdf
815195	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_Trouble_Shooting_Linux_USB_Host_UVC_CN.pdf
140255	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/WATCHDOG/Rockchip_Developer_Guide_Linux_WDT_CN.pdf
134021	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/WATCHDOG/Rockchip_Developer_Guide_Linux_WDT_EN.pdf
786621	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Debian_CN.pdf
111818	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Debian_Docker_EN.pdf
183819	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Debian_EN.pdf
407359	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_Docker_Deploy_CN.pdf
742951	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_Flash_Open_Source_Solution_CN.pdf
232109	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_Flash_Open_Source_Solution_EN.pdf
1103926	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_PCBA_CN.pdf
1000213	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_PCBA_EN.pdf
549012	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Linux_RKIPC_CN.pdf
221297	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Third_Party_System_Adaptation_CN.pdf
132768	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Developer_Guide_Third_Party_System_Adaptation_EN.pdf
299345	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Instruction_Linux_ROS2_CN.pdf
128064	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Instruction_Linux_ROS_CN.pdf
130136	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Instruction_Linux_ROS_EN.pdf
387048	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf
370058	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_EN.pdf
504346	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Use_Guide_Linux_EtherCAT_IgH_CN.pdf
1513512	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_Use_Guide_Linux_RetroArch_CN.pdf
514856	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_User_Guide_Linux_RetroArch_EN.pdf
321844	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/ApplicationNote/Rockchip_User_Guide_SDK_Docker_CN.pdf
5921257	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Algorithms/Rockchip_Developer_Guide_Audio_Algorithm_VQE_Introduction_CN.pdf
173102	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Algorithms/Rockchip_Developer_Guide_RockAA_Utils_CN.pdf
361904	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Algorithms/Rockchip_Developer_Guide_Sound_Event_Detection_CN.pdf
200463	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Algorithms/Rockchip_Developer_Guide_Sound_Event_Detection.pdf
2487419	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Rockchip_Developer_Guide_Microphone_Array_TEST_CN.pdf
5901970	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Rockchip_Developer_Guide_Microphone_Array_Tuning.pdf
199332	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Rockchip_Developer_Guide_PulseAudio_CN.pdf
631839	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Audio/Rockchip_Introduction_Linux_Audio_3A_Algorithm_CN.pdf
538969	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Developer_Guide_Linux4.4_Camera_CN.pdf
331240	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Developer_Guide_Linux4.4_Camera_EN.pdf
1225267	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Developer_Guide_Linux_RMSL_CN.pdf
1129053	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Developer_Guide_Linux_RMSL_EN.pdf
395739	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Trouble_Shooting_Linux4.4_Camera_CN.pdf
275166	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Trouble_Shooting_Linux4.4_Camera_EN.pdf
570860	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Camera/Rockchip_Trouble_Shooting_Linux5.10_Camera_CN.pdf
145373	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Docker/Rockchip_Developer_Guide_Debian_Docker_CN.pdf
209978	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Docker/Rockchip_Developer_Guide_Linux_Docker_Deploy_CN.pdf
321844	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Docker/Rockchip_User_Guide_SDK_Docker_CN.pdf
467033	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/DPDK/Rockchip_Developer_Guide_Linux_DPDK_CN.pdf
142099	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/DPDK/Rockchip_Guide_Linux_DPDK_CN.pdf
224667	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Graphics/Rockchip_Developer_Guide_Buildroot_Weston_CN.pdf
99345	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Graphics/Rockchip_Developer_Guide_Buildroot_Weston_EN.pdf
325560	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Graphics/Rockchip_Developer_Guide_Linux_Graphics_CN.pdf
635052	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Graphics/Rockchip_Developer_Guide_Linux_Graphics_EN.pdf
813375	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Graphics/Rockchip_Developer_Guide_Linux_LVGL_CN.pdf
759647	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_Linux_RKADK_CN.pdf
2213623	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_CN.pdf
1821890	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_EN.pdf
777921	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_CN.pdf
239453	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_EN.pdf
642190	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_CN.pdf
141130	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_EN.pdf
631839	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Introduction_Linux_Audio_3A_Algorithm_CN.pdf
428576	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Introduction_Linux_Audio_3A_Algorithm_EN.pdf
522084	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_User_Guide_Linux_Gstreamer_CN.pdf
144933	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_User_Guide_Linux_Gstreamer_EN.pdf
261768	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_User_Guide_Linux_Rockit_CN.pdf
1141996	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_Developer_Guide_Linux_PCBA_CN.pdf
153407	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_Introduction_Linux_Benchmark_KPI_CN.pdf
107841	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_Introduction_Linux_Benchmark_KPI_EN.pdf
609815	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_Introduction_Linux_PLT_CN.pdf
547371	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_Introduction_Linux_PLT_EN.pdf
429115	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_User_Guide_Linux_Software_Test_CN.pdf
1047159	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Profile/Rockchip_User_Guide_Linux_Software_Test_EN.pdf
461674	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Developer_Guide_Linux_DFU_Upgrade_CN.pdf
1365292	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Developer_Guide_Linux_Recovery_CN.pdf
1250005	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Developer_Guide_Linux_Recovery_EN.pdf
593470	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Developer_Guide_Linux_Upgrade_CN.pdf
316716	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Developer_Guide_Linux_Upgrade_EN.pdf
376510	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Recovery/Rockchip_Introduction_Smart_Screen_OTA_CN.pdf
572956	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Security/Rockchip_Developer_Guide_Crypto_HWRNG_CN.pdf
1165086	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Security/Rockchip_Developer_Guide_Linux_Secure_Boot_CN.pdf
714546	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Security/Rockchip_Developer_Guide_Linux_Secure_Boot_EN.pdf
972964	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Security/Rockchip_Developer_Guide_TEE_SDK_CN.pdf
635600	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/System/Rockchip_Developer_Guide_Buildroot_CN.pdf
755670	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/System/Rockchip_Developer_Guide_Debian_CN.pdf
195419	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/System/Rockchip_Developer_Guide_Third_Party_System_Adaptation_CN.pdf
325538	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Uefi/Rockchip_Developer_Guide_UEFI_CN.pdf
469411	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/AP模组RF测试文档/BT RF Test Commands for Linux-v05.pdf
3640735	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/AP模组RF测试文档/Wi-Fi RF Test Commands for Linux-v03.pdf
948224	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/REALTEK模组RF测试文档/00014010-WS-170731-RTL8723D_COB_MP_FLOW_R04.pdf
1907712	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/REALTEK模组RF测试文档/MP tool user guide for linux20180319.pdf
674812	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/RK平台_RTL8723DS_AIRKISS配网说明.pdf
820221	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_DeviceIo_Bluetooth_CN.pdf
437999	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_DeviceIo_Bluetooth_EN.pdf
1044763	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_Linux_WIFI_BT_CN.pdf
970332	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_Linux_WIFI_BT_EN.pdf
1271337	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_Network_Config_CN.pdf
1173966	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Developer_Guide_Network_Config_EN.pdf
321459	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Introduction_RK3308_DeviceIo_WIFI_CN.pdf
181561	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/Rockchip_Introduction_RK3308_DeviceIo_WIFI_EN.pdf
674812	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/WIFIBT编程接口/RK平台_RTL8723DS_AIRKISS配网说明.pdf
820221	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/WIFIBT编程接口/Rockchip_Developer_Guide_DeviceIo_Bluetooth_CN.pdf
1271337	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/WIFIBT编程接口/Rockchip_Developer_Guide_Network_Config_CN.pdf
321459	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Wifibt/WIFIBT编程接口/Rockchip_Introduction_RK3308_DeviceIo_WIFI_CN.pdf
3103928	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Others/Rockchip_Developer_Guide_Linux_Software_CN.pdf
147127	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Others/Rockchip_Developer_Guide_Repo_Mirror_Server_Deploy_CN.pdf
987262	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Others/Rockchip_Trouble_Shooting_Linux_Real-Time_Performance_CN.pdf
1010868	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Others/Rockchip_User_Guide_Bug_System_CN.pdf
1609790	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Others/Rockchip_User_Guide_SDK_Application_And_Synchronization_CN.pdf
2048854	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3528/Rockchip RK3528 Datasheet V1.0-20230522.pdf
1924915	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip_RK3562_Datasheet-V1.2-20240428.pdf
1917561	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 Datasheet-V2.0-20241104.pdf
1919435	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562J Datasheet-V2.0-20241104.pdf
29340263	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part1-20230803.pdf
38786467	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part2-20230803.pdf
604879	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/RK3568 Brief Datasheet.pdf
3527699	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.2-20210601.pdf
2943148	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf
2727431	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568B2_Datasheet-V2.1-20240621.pdf
2466201	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V1.3-20220929P.PDF
2357471	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf
2778867	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V1.0-20220304.pdf
2734375	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V2.1-20240621.pdf
11814295	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_TRM_Part1_V1.3-20220930P.PDF
125198915	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip RK3568 TRM Part2 V1.1-20210301.pdf
930542	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/RK3576_Brief_Datasheet_V1.3-20240315.pdf
2509967	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_Datasheet_V1.1-20240430.pdf
40000071	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part1_V1.2_20240624.pdf
81034667	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part2_V1.2_20240624.pdf
2347113	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 Datasheet V1.1-20220124.pdf
2138731	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588_Datasheet_V1.7-20231117.pdf
3637423	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588J_Datasheet_V1.1-20230803.pdf
3500475	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588M_Datasheet_V1.1-20230803.pdf
2105004	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588S2 Datasheet V1.0-20231101.pdf
2105004	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S2_Datasheet_V1.0-20231101.pdf
1429411	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.4-20220621.pdf
3148862	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.5-20230117.pdf
56601220	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part1-20220309.pdf
55558880	/mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part2 20220309.pdf
781394	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/SCH/LubanCat5CB_EBF410152V1R0_SCH_20240820.pdf
730847	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/SCH/LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf
40611	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/尺寸图/LubanCat5CB_EBF410152V1R0_尺寸图_20240820.pdf
185756	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/尺寸图/LubanCat5IOB_EBF410153V1R0_尺寸图_20240820.pdf
150980	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5CB_EBF410152V1R0_位号文件_20240820.pdf
132917	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5CB_EBF410152V1R0_器件值文件_20240820.pdf
265729	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_位号文件_20240820.pdf
218729	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_器件值文件_20240820.pdf
24901601	/mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/[野火]鲁班猫5BTB开发板硬件规格书_V1.1.3.pdf
```
</details>

## 2. pdfinfo on name matches

`pdfinfo` is installed (`/usr/bin/pdfinfo`). Files whose **base name** matches
`grep -iE 'rk3588|trm|datasheet|vop|mipi|dsi|hdmi|display|gpu|mali|rga|mpp|hardware|schematic|5io'`:
76 files (69 under `lubancat/`, 7 under `Alpha_imx6ull/`). The command for each one:

```console
$ pdfinfo <file> | grep -E 'Title|Author|Creator|Pages|CreationDate'
```

<details><summary>Verbatim output (### = file)</summary>

```
### /mnt/hgfs/vm_share/Alpha_imx6ull/02、开发板原理图/正点原子其他模块原理图/ATK-RGB2HDMI模块V1.3.pdf
Title:           New Output
Author:          Windows 9x/NT/2000/XP User
Creator:         Altium Designer
CreationDate:    Mon Apr 27 08:00:00 2020 CST
Pages:           1
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/eMMC MKEMB008GT1E-C Datasheet_Version_01 00_20210412.pdf
Title:           MKEMB008GT1E-C
Author:          Ken Lee
Creator:         Microsoft® Word 2019
CreationDate:    Wed Dec 29 00:53:07 2021 CST
Pages:           41
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/FT5426(电容触摸屏IC)/FT5x06 - 1005 DataSheet - Preliminary.pdf
Title:           Microsoft Word - FT5x06 - 1005 DataSheet.doc
Author:          swtung
Creator:         PScript5.dll Version 5.2.2
CreationDate:    Fri Jun 11 14:13:54 2010 CST
Pages:           17
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/ICM-20608-G Datasheet Revision 1.0.pdf
Author:          Administrator
Creator:         Microsoft® Word 2010
CreationDate:    Sat Sep  5 02:33:55 2015 CST
Pages:           35
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/LAN_8720A-CP_datasheet.pdf
Title:           The DatasheetArchive - Datasheet Search Engine
Author:          The DatasheetArchive
CreationDate:    Tue May 25 00:13:12 2010 CST
Pages:           79
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/SR8201F datasheet.pdf
Title:           ds_sr8201f_vb_en_v1.1.doc
Author:          shenhanbing
Creator:         pdfFactory Pro www.pdffactory.com
CreationDate:    Wed Dec 27 19:53:06 2017 CST
Pages:           51
### /mnt/hgfs/vm_share/Alpha_imx6ull/06、硬件资料/01、芯片资料/YT8512_Datasheet_v1.05.pdf
Title:           MotorComm YT8512C YT8512H Datasheet
Author:          REV v1.0
Creator:         Microsoft® Word 2013
CreationDate:    Wed Mar 11 10:03:50 2020 CST
Pages:           73
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_Developer_Guide_DisplayPort_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:34:43 2024 CST
Pages:           48
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_DisplayPort_SI_Test_Guide_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:38:47 2024 CST
Pages:           13
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3399_Developer_Guide_DisplayPort_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:39:19 2024 CST
Pages:           33
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 17 15:34:50 2022 CST
Pages:           24
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/DRM/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:35:07 2024 CST
Pages:           57
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDCP/Rockchip_RK3588_Developer_Guide_HDCP_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:39:29 2024 CST
Pages:           19
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:36:15 2024 CST
Pages:           39
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:35:50 2024 CST
Pages:           50
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf
Creator:         Typora
CreationDate:    Tue Jul  7 16:44:09 2020 CST
Pages:           13
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_Developer_Guide_MIPI_DSI2_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:36:43 2024 CST
Pages:           69
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Aug 20 16:11:15 2022 CST
Pages:           63
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Aug 20 16:06:32 2022 CST
Pages:           52
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf
Creator:         Typora
CreationDate:    Sun Sep 27 17:47:31 2020 CST
Pages:           38
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI_CN.pdf
Creator:         Typora
CreationDate:    Sun Sep 27 17:46:50 2020 CST
Pages:           59
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf
Creator:         Typora
CreationDate:    Tue Jul  7 16:44:09 2020 CST
Pages:           13
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_DRM_Display_Driver_Development_Guide_V1.0.pdf
Title:           
Author:          USER-
Creator:         WPS 文字
CreationDate:    Mon Jan 21 16:00:22 2019 CST
Pages:           28
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 17 15:34:50 2022 CST
Pages:           24
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Aug 20 16:11:15 2022 CST
Pages:           63
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Aug 20 16:17:53 2022 CST
Pages:           6
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_VOP2_Plane_Assign.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Jan 10 14:34:39 2022 CST
Pages:           12
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/DISPLAY/Vsync/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:39:38 2024 CST
Pages:           11
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_IN_Based_On_CameraHal3_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:48:01 2024 CST
Pages:           32
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_RX_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 15:48:12 2024 CST
Pages:           46
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/MPP/Rockchip_Developer_Guide_MPP_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Jan  4 16:16:17 2025 CST
Pages:           48
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RGA/Rockchip_Developer_Guide_RGA_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Jan  4 16:18:05 2025 CST
Pages:           65
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RGA/Rockchip_FAQ_RGA_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Sat Jan  4 16:18:41 2025 CST
Pages:           42
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_Display_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Wed Sep 25 17:24:59 2024 CST
Pages:           27
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3588_Developer_Guide_System_Suspend_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 18:36:08 2024 CST
Pages:           11
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3588_Developer_Guide_USB_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Mon Dec 30 18:45:33 2024 CST
Pages:           47
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_CN.pdf
Title:           MPP 开发参考_v0.5
Author:          Herman
Creator:         Microsoft® Word 2010
CreationDate:    Wed Jun 10 11:08:59 2020 CST
Pages:           38
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_EN.pdf
Title:           Rockchip_Developer_Guide_MPP_EN_v0.5
Author:          HermanChen;Lily Chen
Creator:         Microsoft® Word 2010
CreationDate:    Thu Jun 11 18:15:30 2020 CST
Pages:           39
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 24 17:03:47 2022 CST
Pages:           61
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_EN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 24 17:04:20 2022 CST
Pages:           61
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_CN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 24 17:04:53 2022 CST
Pages:           23
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_EN.pdf
Title:           
Author:          
Creator:         Typora
CreationDate:    Fri Jun 24 17:05:09 2022 CST
Pages:           23
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3528/Rockchip RK3528 Datasheet V1.0-20230522.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Wed May 24 16:24:48 2023 CST
Pages:           28
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip_RK3562_Datasheet-V1.2-20240428.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Mon Apr 29 17:27:30 2024 CST
Pages:           30
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 Datasheet-V2.0-20241104.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Mon Nov  4 10:30:06 2024 CST
Pages:           30
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562J Datasheet-V2.0-20241104.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Mon Nov  4 10:17:28 2024 CST
Pages:           30
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part1-20230803.pdf
Title:           
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Thu Aug  3 15:42:03 2023 CST
Pages:           1330
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part2-20230803.pdf
Title:           
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Thu Aug  3 15:31:34 2023 CST
Pages:           1677
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/RK3568 Brief Datasheet.pdf
Title:           RK3568 Brief Datasheet
Author:          LIU Mingmiao
Creator:         Microsoft® Word 2021
CreationDate:    Tue May 24 09:18:08 2022 CST
Pages:           3
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.2-20210601.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Thu Jun  3 18:28:34 2021 CST
Pages:           62
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Jun 21 10:50:23 2024 CST
Pages:           61
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568B2_Datasheet-V2.1-20240621.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Jun 21 11:10:43 2024 CST
Pages:           61
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V1.3-20220929P.PDF
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Sat Oct  8 20:46:02 2022 CST
Pages:           61
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Jun 21 11:04:30 2024 CST
Pages:           62
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V1.0-20220304.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Mon Mar  7 19:34:28 2022 CST
Pages:           62
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V2.1-20240621.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Jun 21 11:14:33 2024 CST
Pages:           62
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_TRM_Part1_V1.3-20220930P.PDF
Title:           Rockchip RK3568 TRM Part1 V1.3-20220930P
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Sat Oct  8 18:04:18 2022 CST
Pages:           852
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip RK3568 TRM Part2 V1.1-20210301.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Sun Feb 28 21:56:30 2021 CST
Pages:           2583
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/RK3576_Brief_Datasheet_V1.3-20240315.pdf
Title:           RK3588 Brief Datasheet
Author:          LIU Mingmiao
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Mar 15 14:16:47 2024 CST
Pages:           3
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_Datasheet_V1.1-20240430.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Tue Apr 30 10:34:04 2024 CST
Pages:           47
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part1_V1.2_20240624.pdf
Title:           
Author:          PDF文件处理助手
Creator:         PDF文件处理助手
CreationDate:    Mon Jun 24 17:12:53 2024 CST
Pages:           1412
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part2_V1.2_20240624.pdf
Author:          PDF文件处理助手
Creator:         PDF文件处理助手
CreationDate:    Tue Jun 25 09:52:06 2024 CST
Pages:           2969
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 Datasheet V1.1-20220124.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word 2016
CreationDate:    Wed Jan 26 17:37:22 2022 CST
Pages:           36
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588_Datasheet_V1.7-20231117.pdf
Title:           Rockchip RK3588 Datasheet V1.5
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Fri Nov 17 18:14:34 2023 CST
Pages:           34
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588J_Datasheet_V1.1-20230803.pdf
Title:           Rockchip RK3588J Datasheet V0.6
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Tue Aug  8 09:43:54 2023 CST
Pages:           35
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588M_Datasheet_V1.1-20230803.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Tue Aug  8 09:42:24 2023 CST
Pages:           34
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588S2 Datasheet V1.0-20231101.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Wed Nov  1 15:18:04 2023 CST
Pages:           35
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S2_Datasheet_V1.0-20231101.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Wed Nov  1 15:18:04 2023 CST
Pages:           35
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.4-20220621.pdf
Title:           Rockchip RK3588S Datasheet V1.4
Author:          Rockchip
Creator:         Microsoft® Word 2021
CreationDate:    Tue Jun 21 11:46:14 2022 CST
Pages:           35
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.5-20230117.pdf
Title:           .
Author:          Rockchip
Creator:         福昕软件开发股份有限公司
CreationDate:    Mon Jul 31 15:50:59 2023 CST
Pages:           35
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part1-20220309.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word for Microsoft 365
CreationDate:    Tue Mar 15 19:57:48 2022 CST
Pages:           2287
### /mnt/hgfs/vm_share/lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part2 20220309.pdf
Title:           .
Author:          Rockchip
Creator:         Microsoft® Word for Microsoft 365
CreationDate:    Tue Mar 15 21:40:24 2022 CST
Pages:           3706
### /mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/SCH/LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf
CreationDate:    Wed Aug 21 11:48:04 2024 CST
Pages:           18
### /mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/尺寸图/LubanCat5IOB_EBF410153V1R0_尺寸图_20240820.pdf
Creator:         Allegro Design Publisher
CreationDate:    Wed Aug 21 10:51:05 2024 CST
Pages:           2
### /mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_位号文件_20240820.pdf
Creator:         Allegro Design Publisher
CreationDate:    Tue Aug 20 10:50:05 2024 CST
Pages:           2
### /mnt/hgfs/vm_share/lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_器件值文件_20240820.pdf
Creator:         Allegro Design Publisher
CreationDate:    Tue Aug 20 10:51:20 2024 CST
Pages:           2
```
</details>

## 3. Classification (file name + pdfinfo only)

| File (under `/mnt/hgfs/vm_share/`) | Pages | Type | RK3588? |
|---|---|---|---|
| `Alpha_imx6ull/02、开发板原理图/正点原子其他模块原理图/ATK-RGB2HDMI模块V1.3.pdf` | 1 | none of these (developer guide / other) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/eMMC MKEMB008GT1E-C Datasheet_Version_01 00_20210412.pdf` | 41 | Datasheet: not a Rockchip SoC (by name) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/FT5426(电容触摸屏IC)/FT5x06 - 1005 DataSheet - Preliminary.pdf` | 17 | Datasheet: not a Rockchip SoC (by name) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/ICM-20608-G Datasheet Revision 1.0.pdf` | 35 | Datasheet: not a Rockchip SoC (by name) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/LAN_8720A-CP_datasheet.pdf` | 79 | Datasheet: not a Rockchip SoC (by name) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/SR8201F datasheet.pdf` | 51 | Datasheet: not a Rockchip SoC (by name) | no |
| `Alpha_imx6ull/06、硬件资料/01、芯片资料/YT8512_Datasheet_v1.05.pdf` | 73 | Datasheet: not a Rockchip SoC (by name) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_Developer_Guide_DisplayPort_CN.pdf` | 48 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_DisplayPort_SI_Test_Guide_CN.pdf` | 13 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3399_Developer_Guide_DisplayPort_CN.pdf` | 33 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/DP/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf` | 24 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/DRM/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf` | 57 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/HDCP/Rockchip_RK3588_Developer_Guide_HDCP_CN.pdf` | 19 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf` | 39 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI_CN.pdf` | 50 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/HDMI/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf` | 13 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_Developer_Guide_MIPI_DSI2_CN.pdf` | 69 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/MIPI/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf` | 63 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf` | 52 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf` | 38 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI_CN.pdf` | 59 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_Developer_Guide_HDMI-PHY-PLL_Config_CN.pdf` | 13 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_DRM_Display_Driver_Development_Guide_V1.0.pdf` | 28 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_DisplayPort_CN.pdf` | 24 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_MIPI_DSI2_CN.pdf` | 63 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf` | 6 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Rockchip_VOP2_Plane_Assign.pdf` | 12 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/DISPLAY/Vsync/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf` | 11 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_IN_Based_On_CameraHal3_CN.pdf` | 32 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/HDMI-IN/Rockchip_Developer_Guide_HDMI_RX_CN.pdf` | 46 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/MPP/Rockchip_Developer_Guide_MPP_CN.pdf` | 48 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/RGA/Rockchip_Developer_Guide_RGA_CN.pdf` | 65 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/RGA/Rockchip_FAQ_RGA_CN.pdf` | 42 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/RTT/Rockchip_Developer_Guide_RT-Thread_Display_CN.pdf` | 27 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/TRUST/Rockchip_RK3588_Developer_Guide_System_Suspend_CN.pdf` | 11 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Common/USB/Rockchip_RK3588_Developer_Guide_USB_CN.pdf` | 47 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_CN.pdf` | 38 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_MPP_EN.pdf` | 39 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_CN.pdf` | 61 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_Developer_Guide_RGA_EN.pdf` | 61 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_CN.pdf` | 23 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/Linux/Multimedia/Rockchip_FAQ_RGA_EN.pdf` | 23 | none of these (developer guide / other) | no |
| `lubancat/5-RockChip官方文档/RK3528/Rockchip RK3528 Datasheet V1.0-20230522.pdf` | 28 | Datasheet: RK3528, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3562/Rockchip_RK3562_Datasheet-V1.2-20240428.pdf` | 30 | Datasheet: RK3562, V1.2 | no |
| `lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 Datasheet-V2.0-20241104.pdf` | 30 | Datasheet: RK3562, V2.0 | no |
| `lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562J Datasheet-V2.0-20241104.pdf` | 30 | Datasheet: RK3562J, V2.0 | no |
| `lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part1-20230803.pdf` | 1330 | TRM: RK3562, Part 1, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3562/Rockchip RK3562 TRM V1.0-Part2-20230803.pdf` | 1677 | TRM: RK3562, Part 2, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/RK3568 Brief Datasheet.pdf` | 3 | Datasheet: RK3568 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.2-20210601.pdf` | 62 | Datasheet: RK3566, V1.2 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf` | 61 | Datasheet: RK3566, V1.4 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568B2_Datasheet-V2.1-20240621.pdf` | 61 | Datasheet: RK3568B2, V2.1 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V1.3-20220929P.PDF` | 61 | Datasheet: RK3568, V1.3 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf` | 62 | Datasheet: RK3568, V2.1 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V1.0-20220304.pdf` | 62 | Datasheet: RK3568J, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568J_Datasheet_V2.1-20240621.pdf` | 62 | Datasheet: RK3568J, V2.1 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip_RK3568_TRM_Part1_V1.3-20220930P.PDF` | 852 | TRM: RK3568, Part 1, V1.3 | no |
| `lubancat/5-RockChip官方文档/RK3566_RK3568/Rockchip RK3568 TRM Part2 V1.1-20210301.pdf` | 2583 | TRM: RK3568, Part 2, V1.1 | no |
| `lubancat/5-RockChip官方文档/RK3576/RK3576_Brief_Datasheet_V1.3-20240315.pdf` | 3 | Datasheet: RK3576, V1.3 | no |
| `lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_Datasheet_V1.1-20240430.pdf` | 47 | Datasheet: RK3576, V1.1 | no |
| `lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part1_V1.2_20240624.pdf` | 1412 | TRM: RK3576, Part 1, V1.2 | no |
| `lubancat/5-RockChip官方文档/RK3576/Rockchip_RK3576_TRM_Part2_V1.2_20240624.pdf` | 2969 | TRM: RK3576, Part 2, V1.2 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 Datasheet V1.1-20220124.pdf` | 36 | Datasheet: RK3588, V1.1 | **yes** |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588_Datasheet_V1.7-20231117.pdf` | 34 | Datasheet: RK3588, V1.7 | **yes** |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588J_Datasheet_V1.1-20230803.pdf` | 35 | Datasheet: RK3588J, V1.1 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588M_Datasheet_V1.1-20230803.pdf` | 34 | Datasheet: RK3588M, V1.1 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588S2 Datasheet V1.0-20231101.pdf` | 35 | Datasheet: RK3588S2, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S2_Datasheet_V1.0-20231101.pdf` | 35 | Datasheet: RK3588S2, V1.0 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.4-20220621.pdf` | 35 | Datasheet: RK3588S, V1.4 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip_RK3588S_Datasheet_V1.5-20230117.pdf` | 35 | Datasheet: RK3588S, V1.5 | no |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part1-20220309.pdf` | 2287 | TRM: RK3588, Part 1, V1.0 | **yes** |
| `lubancat/5-RockChip官方文档/RK3588/Rockchip RK3588 TRM V1.0-Part2 20220309.pdf` | 3706 | TRM: RK3588, Part 2, V1.0 | **yes** |
| `lubancat/鲁班猫5-BTB/20240820_fix1/SCH/LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf` | 18 | Schematic: LubanCat5IOB (EBF410153 V1R0), not named 5IO | no |
| `lubancat/鲁班猫5-BTB/20240820_fix1/尺寸图/LubanCat5IOB_EBF410153V1R0_尺寸图_20240820.pdf` | 2 | LubanCat5IOB board drawing (dimension/assembly), not a schematic or manual | no |
| `lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_位号文件_20240820.pdf` | 2 | LubanCat5IOB board drawing (dimension/assembly), not a schematic or manual | no |
| `lubancat/鲁班猫5-BTB/20240820_fix1/装配图/LubanCat5IOB_EBF410153V1R0_器件值文件_20240820.pdf` | 2 | LubanCat5IOB board drawing (dimension/assembly), not a schematic or manual | no |

Notes:
- "Type" is taken only from the file name. The embedded `Title` is often just `.` and is ignored unless it adds something (as for the RK3588 V1.7 datasheet above).
- Rockchip developer guides (DRM, VOP2 plane assign, MIPI DSI2, HDMI, DP, MPP, RGA) are relevant to the labs but are none of TRM, datasheet or schematic. Their content was not checked.
