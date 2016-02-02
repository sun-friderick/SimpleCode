#!/bin/sh

##ubuntu / linux mint系统所需常用软件




##=============================================================
# 代码make工具：cmake ccmake rake
sudo apt-get install cmake cmake-curses-gui rake  -y

# 版本控制工具：subversion  git
sudo apt-get install subversion  git -y

# 比较工具：meld diff viewer
sudo apt-get install meld  diff  viewer  -y

# 压缩、解压工具：rar unrar zip unzip
sudo apt-get install  rar  unrar  zip u nzip  -y




##=============================================================
# 屏幕管理器： tmux； 
sudo apt-get install tmux -y

# 终端增强工具： Terminator， yakuake(不适用linux mint环境)，    
sudo apt-get install terminator   -y   

# 开启关闭服务：sysv-rc-conf; 
sudo apt-get install sysv-rc-conf  -y

# 系统信息：sysstat sysinfo；
sudo apt-get install sysstat sysinfo -y

#交互式进程查看器：htop；
sudo apt-get install htop -y

# 系统负载指示器：indicator-multiload；  
sudo apt-get install indicator-multiload -y

# 不在使用鼠标： keynav；
sudo apt-get install keynav -y


# 阻止休眠:Caffeine
sudo add-apt-repository ppa:caffeine-developers/ppa
sudo apt-get update
sudo apt-get install caffeine




##=============================================================
# 浏览器 chromium；
sudo apt-get install chromium-browser -y

# 文本浏览器： w3m； 
sudo apt-get install w3m -y



##=============================================================
# samba链接：
sudo apt-get install samba -y

# ssh链接：
sudo apt-get install ssh openssh-server  -y



##=============================================================
# 安装 搜狗输入法&谷歌输入法   安装SopCast Player  
#sudo add-apt-repository  ppa:fcitx-team/nightly  
#sudo add-apt-repository ppa:fcitx-team/dailybuild-fcitx-master 
#sudo apt-get update 
#sudo apt-get install  fcitx fcitx-config-gtk fcitx-sogoupinyin fcitx-sogoupinyin  -y



##=============================================================
# 编辑器：gedit，Atom；
sudo apt-add-repository  ppa:webupd8team/atom            # 编辑器：gedit，Atom；
sudo add-apt-repository  ppa:webupd8team/sublime-text-3  # 编辑器：sublime-text-3
sudo add-apt-repository  ppa:nvbn-rm/ppa        # 笔记：Evernote；
sudo add-apt-repository  ppa:diodon-team/stable # 剪贴板管理工具：Diodon；
sudo apt-get update 
sudo apt-get install atom gedit -y              # 编辑器：gedit，Atom；
sudo apt-get install sublime-text-installer -y  # 编辑器：sublime-text-3
sudo apt-get install everpad -y  # 笔记：Evernote；
sudo apt-get install diodon -y   # 剪贴板管理工具：Diodon；






