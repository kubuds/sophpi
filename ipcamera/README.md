# how to build and use ipcamera 
1. download sophapp (git clone ssh://long.xu@10.240.0.84:29418/sophapp.git --branch=ipcamera)
2. copy sophapp to root of the release SDK
3. source build/envsetup_soc.sh
4. make xxx_defconfig;make ipcamera clean;make ipcamera;make ipcamera install
5. run APP on board; example: "./ipcam_cv180x -i param_config_ai.ini &"
## Notes:
 * source build environment before build ipcam
 * most parameters can modify in *.ini files
------------------------------------------------------------------------------------
# ipcam folder tree overview
| Folder/File | Description                                             |
| ----------- | ------------------------------------------------------- |
| config      | set your build parameter                                |
| main        | app entry                                               |
| Makefile    | build app and output ipcam_cv18xx                       |
| modules     | include video audio AI OSD ...                          |
| prebuilt    | cvitek libs and 3rd libs                                |
| README.md   | notes                                                   |
| resource    | include ai models , parameter config file , and so on   |

#编译方式 (这里用ipcamer这个solutions作为示例)
1. 选择对应板卡配置 make xxx_defconfig
2. 编译make ipcamera clean;make ipcamera;
3，安装可执行程序和资源到install下 make ipcamera install

#添加对应宏方式
1.将对应宏 + CONFIG_XXXX 配置 放置于dep.mk中

#添加模块方式
1.module 目录下对应添加 Makefile 以及src,include等目录 具体参考module/ai
2.如果需要全局依赖的头文件目录 则放到dep.mk中，如果为模块内需要依赖的头文件则对应放置于模块Makefile中
3.在link.mk中添加对应生成的libxxxx.a 作为依赖
4.在build/module/Kconfig 中添加对应的模块配置
5.在configs/xxxx_defconfig 添加&打开对应的模块配置

#添加对应安装的方式
1.在build/resouce/Kconfig中添加对应的配置
2.在install.mk中对应添加

#Version v1.0.0 改动点
1.支持Kconfig动态配置
2.拆分link.mk & dep.mk & install.mk
3.拆除耦合SDK build/.config  使用sensor 和 panel需要在sophapp中进行配置， 默认配置无支持Sensor 和Panel

#Version v1.0.1 改动点
1.支持多路录像， 支持录像配置，绑定VencChn，多路复用
