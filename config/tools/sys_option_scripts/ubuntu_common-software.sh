#!/bin/sh

##ubuntu / linux mint系统所需常用软件
# 可参考 http://blog.codinglabs.org/articles/getting-started-with-ubuntu.html

##===============================================================================================
##===============================================================================================
# 浏览器 chromium；
sudo apt-get install chromium-browser -y

# 文本浏览器： w3m； 
sudo apt-get install w3m -y



##===============================================================================================
##===============================================================================================
# 音视频、图形图像 
    
# 图像编辑：gimp；
sudo apt-get install gimp  -y  

# 截图抓屏工具:shutter ksnapshot
sudo apt-get install shutter ksnapshot -y

# 视频播放：vlc  
sudo apt-get install vlc xine -y

# 看图：gThumb
sudo apt-get install gThumb -y 

# 网络电视：gmlive
sudo apt-get install gmlive gsopcast -y




##===============================================================================================
##===============================================================================================
# 电子书pdf查看软件

# PDF查看编辑： xpdf xpdf-chinese-simplified
sudo apt-get install xpdf xpdf-chinese-simplified -y  
sudo apt-get install  poppler-data  #ubuntu evince pdf 中文 乱码 方框

# 漫画查看：Comix
sudo apt-get install Comix -y

# 电子书管理：Calibre
sudo apt-get install Calibre -y




##===============================================================================================
##===============================================================================================
# 网络工具

# 下载工具：wget thunderbird
sudo apt-get install wget thunderbird -y

# 抓包工具：
sudo apt-get install  wireshark -y

# VPN工具：
sudo apt-get install  vpnc -y

# NFS链接：
sudo apt-get install  nfs-common nfs-kernel-server -y

# samba链接：
sudo apt-get install samba -y

# ftp链接：
sudo apt-get install tftp  -y

# ssh链接：
sudo apt-get install ssh openssh-server  -y

# ssh跳墙时使用：plink；举例plink -C -D 127.0.0.1:43210 -N -pw 12345 kashu@8.8.8.8
sudo apt-get install plink  -y

# 科学上网Shadowsocks;仅支持Ubuntu 14.04或更高版本
sudo add-apt-repository ppa:hzwhuang/ss-qt5
sudo apt-get update
sudo apt-get install shadowsocks-qt5 -y



##===============================================================================================
##===============================================================================================
# 安装 搜狗输入法&谷歌输入法   安装SopCast Player  
sudo add-apt-repository  ppa:fcitx-team/nightly  
sudo add-apt-repository ppa:fcitx-team/dailybuild-fcitx-master 
sudo apt-get update 
sudo apt-get install  fcitx fcitx-config-gtk fcitx-sogoupinyin fcitx-sogoupinyin  -y



##===============================================================================================
##===============================================================================================
# 编辑器：gedit，Atom；
sudo apt-add-repository  ppa:webupd8team/atom
sudo apt-get update 
sudo apt-get install atom gedit -y

# 编辑器：sublime-text-3
sudo add-apt-repository  ppa:webupd8team/sublime-text-3 
sudo apt-get update 
sudo apt-get install sublime-text-installer -y

# 笔记：Evernote；
sudo add-apt-repository  ppa:nvbn-rm/ppa 
sudo apt-get update 
sudo apt-get install everpad -y

# 剪贴板管理工具：Diodon；
sudo add-apt-repository  ppa:diodon-team/stable
sudo apt-get update 
sudo apt-get install diodon -y

# 查看 && 安装中文字体
fc-list :lang=zh
sudo apt-get install ttf-wqy-microhei -y


# 做密码管理:Keepass2; 官网（http://keepass.info/translations.html）
#sudo apt-get install keepass2
#sudo apt-get install mono-dmcs libmono-system-management4.0-cil  #Firefox用户,接着安装对应的KeeFox扩展
#sudo apt-get install binfmt-support cli-common libgdiplus libmono-accessibility4.0-cil libmono-corlib4.5-cil \
                     libmono-data-tds4.0-cil libmono-i18n-west4.0-cil libmono-i18n4.0-cil libmono-posix4.0-cil \
                     libmono-security4.0-cil libmono-system-configuration4.0-cil libmono-system-data4.0-cil  \
                     libmono-system-drawing4.0-cil libmono-system-enterpriseservices4.0-cil \
                     libmono-system-runtime-serialization-formatters-soap4.0-cil  libmono-system-security4.0-cil \
                     libmono-system-transactions4.0-cil libmono-system-windows-forms4.0-cil libmono-system-xml4.0-cil \
                     libmono-system4.0-cil libmono-webbrowser4.0-cil mono-4.0-gac mono-gac  \
                     mono-runtime mono-runtime-common mono-runtime-sgen mono-dmcs libmono-system-management4.0-cil   \
                     libmono-csharp4.0c-cil libmono-microsoft-csharp4.0-cil libmono-system-configuration-install4.0-cil \
                     libmono-system-core4.0-cil  mono-mcs libmono-system-net-http-formatting4.0-cil \
                     libmono-system-net-http4.0-cil  libmono-system-runtime-serialization4.0-cil \
                     libmono-system-xml-linq4.0-cil   #Chrome用户，接着安装对应的Keepasshttp和ChromeIPass扩展


