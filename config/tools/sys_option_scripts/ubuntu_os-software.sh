#!/bin/sh

##ubuntu / linux mint系统所需常用软件

# 系统软件

##===============================================================================================
##===============================================================================================
# 屏幕管理器： tmux； 
sudo apt-get install tmux -y

# 终端增强工具： Terminator， yakuake， putty    
sudo apt-get install terminator  yakuake  putty -y   

# 右键菜单加入打开终端： nautilus-open-terminal；
sudo apt-get install nautilus-open-terminal  -y && sudo nautilus -q && nautilus&



##===============================================================================================
##===============================================================================================
# 开启关闭服务：sysv-rc-conf; 
sudo apt-get install sysv-rc-conf  -y

# 系统信息：sysstat sysinfo；交互式进程查看器：htop；
sudo apt-get install htop sysstat sysinfo -y

# 系统负载指示器：indicator-multiload；  
sudo apt-get install indicator-multiload -y

# 系统监控：conky-all；
sudo apt-get install conky-all  -y

# 启动管理器: startupmanager ；
#sudo apt-get install startupmanager -y

# 双系统时，linux中挂载windows分区
#sudo apt-get install ntfs-config -y
#sudo ntfs-config # 先卸掉已经加载的ntfs卷，再运行这条命令




##===============================================================================================
##===============================================================================================
# 串口调试： gtkterm ；putty终端
sudo apt-get install gtkterm putty -y

# 不在使用鼠标： keynav；
sudo apt-get install keynav -y



##===============================================================================================
##===============================================================================================
# 屏幕widget：screenlets；
sudo add-apt-repository  ppa:screenlets/ppa 
sudo apt-get update  
apt-get install screenlets -y

# 系统优化管理：ubuntu-tweak；
sudo add-apt-repository  ppa:tualatrix/ppa 
sudo apt-get update  
apt-get install ubuntu-tweak -y

# 任务栏显示网速：indicator-netspeed；     
sudo add-apt-repository  ppa:nilarimogard/webupd8 
sudo apt-get update 
apt-get install indicator-netspeed  -y

# 阻止休眠:Caffeine
sudo add-apt-repository ppa:caffeine-developers/ppa
sudo apt-get update
sudo apt-get install caffeine








