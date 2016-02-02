#!/bin/sh

##ubuntu / linux mint系统开发环境所需常用软件
sudo ./ubuntu_update.sh


# 系统软件
# 浏览器 chromium；文本浏览器： w3m； 屏幕管理器： tmux； 终端增强工具： Terminator， yakuake ； 
# 性能监控： conky；不在使用鼠标： keynav；启动管理器: startupmanager；串口调试： gtkterm ；
# 右键菜单加入打开终端： nautilus-open-terminal；开启关闭服务：sysv-rc-conf; 交互式进程查看器：htop；
# 系统负载指示器：indicator-multiload；  putty         
sudo apt-get install chromium-browser w3m terminator tmux yakuake \
                     keynav startupmanager gtkterm htop sysstat sysinfo  \
                     nautilus-open-terminal sysv-rc-conf indicator-multiload putty && sudo nautilus -q && nautilus&      

                     
                     
# 屏幕widget：screenlets；系统优化管理：ubuntu-tweak；系统美化：conky-all；任务栏显示网速：indicator-netspeed；     
sudo add-apt-repository  ppa:screenlets/ppa  
sudo add-apt-repository  ppa:tualatrix/ppa   
sudo add-apt-repository  ppa:nilarimogard/webupd8 
sudo apt-get update 
apt-get install screenlets ubuntu-tweak indicator-netspeed conky-all 



# 安装 搜狗输入法&谷歌输入法   安装SopCast Player  
sudo add-apt-repository  ppa:fcitx-team/nightly  
sudo add-apt-repository ppa:fcitx-team/dailybuild-fcitx-master 
sudo apt-get update 
sudo apt-get install  fcitx fcitx-config-gtk fcitx-sogoupinyin fcitx-sogoupinyin 



# 安装编辑器：gedit，Atom；笔记：Evernote；剪贴板管理工具：Diodon；查看 && 安装中文字体
sudo add-apt-repository  ppa:nvbn-rm/ppa 
sudo add-apt-repository  ppa:webupd8team/sublime-text-3 
sudo add-apt-repository  ppa:diodon-team/stable
sudo apt-add-repository  ppa:webupd8team/atom
sudo apt-get update && fc-list :lang=zh
sudo apt-get install gedit atom everpad sublime-text-installer diodon ttf-wqy-microhei    




# 音视频、图形图像、pdf查看软件
# 截图工具:shutter;    
sudo apt-get install gimp vlc shutter ksnapshot gThumb gmlive xpdf xpdf-chinese-simplified   xine Comix gsopcast    



# 网络工具
# ssh跳墙时使用：plink；举例plink -C -D 127.0.0.1:43210 -N -pw 12345 kashu@8.8.8.8
sudo apt-get install wget thunderbird wireshark nfs-common vpnc tftp samba nfs-kernel-server openssh-server plink    
    

    
# 编译环境
sudo apt-get install cmake make-curses-gui subversion openssh-server git rake manpages-zh  



# 比较工具
sudo apt-get install meld diff viewer rar zip 

sudo apt-get --reinstall install     #重新安装 

# install gcc/g++ first
#sudo apt-get install gcc -y      # if there is no gcc, do this first.
#sudo apt-get install g++ -y      # if there is no g++, do this first.
sudo apt-get  install  build-essential -y && gcc--version   # build-essential依赖: libc6-dev  <libc-dev>libc6-dev  gcc  g++  make  dpkg-dev


sudo 

sudo 

sudo 







# 全盘备份  
# 除去被 –exclude 的目录都会被打进最终的压缩包
mkdir ~/BACKUP_SYSTEM
sudo tar --exclude /proc --exclude /mnt --exclude /tmp --exclude /media --exclude /home/Frederick/Downloads --exclude /home/Frederick/Templates --exclude '/home/Frederick/VirtualBox VMs' --exclude /home/Frederick/BACKUP_SYSTEM -jpcvf ~/BACKUP_SYSTEM/Ubuntu-15.04-20150914-home-pc.tar.bz2 /
watch -d -n 5 ls -alh ~/BACKUP_SYSTEM/   #时时了解压缩的进度

#全盘恢复
#分区软件gparted
sudo apt-get install gparted
sudo gparted



