#!/bin/sh

##ubuntu / linux mint系统开发环境所需常用软件

# 更新系统
sudo apt-get upgrade

# 清除无用的软件包
#在用apt-get install之前,是先将软件包下载到/var/cache/apt/archives中,之后再进行安装的.所以可以先apt-get clean清除/var/cache/apt/archives目录中的软件包.
sudo apt-get clean 

# 更新软件源
sudo apt-get update

# 方便查看已安装的软件包版本号
sudo apt-get install apt-show-versions && apt-show-versions | more  

# 删掉基本不用的自带软件
sudo apt-get remove thunderbird totem rhythmbox empathy brasero simple-scan gnome-mahjongg aisleriot gnome-mines cheese transmission-common gnome-orca webbrowser-app gnome-sudoku landscape-client-ui-install onboard deja-dup 

# 检查是否有损坏的依赖
sudo apt-get check  
#sudo apt-get autoremove --purge #(package 删除包及其依赖的软件包+配置文件等（只对6.10有效，强烈推荐）), --purge清除式卸载



