#!/bin/sh

##ubuntu / linux mint系统所需常用软件


##===============================================================================================
##===============================================================================================
# 编译环境
# 代码make工具：cmake ccmake rake
sudo apt-get install cmake cmake-curses-gui rake  -y

# 版本控制工具：subversion  git
sudo apt-get install subversion  git -y

# 帮助说明： devhelp manpages-zh 
sudo apt-get install devhelp manpages-zh  -y

# 编译工具安装 gcc/g++ 
#sudo apt-get install gcc -y      # if there is no gcc, do this first.
#sudo apt-get install g++ -y      # if there is no g++, do this first.
sudo apt-get  install  build-essential -y && gcc--version   # build-essential依赖: libc6-dev  <libc-dev>libc6-dev  gcc  g++  make  dpkg-dev


# 安装 Python； 可参考 http://blog.csdn.net/kingppy/article/details/13080919
sudo apt-get install  python3.2 python3.2-dev -y # 发布版本，dev包必须安装，很多用pip安装包都需要编译
sudo apt-get install build-essential libssl-dev libevent-dev libjpeg-dev libxml2-dev libxslt-dev -y # 很多pip安装的包都需要libssl和libevent编译环境
sudo apt-get install python-pip -y # 安装 pip
sudo pip install virtualenv  -y # 安装 virtualenv
virtualenv --no-site-packages -p /usr/bin/python3.2 ~/.venv/python3.2  -y # 安装 python3.2 virtualenv



##===============================================================================================
##===============================================================================================
# 比较工具：meld diff viewer
sudo apt-get install meld diff viewer  -y

# 压缩、解压工具：rar zip 
sudo apt-get install  rar unrar zip unzip  -y




sudo apt-get --reinstall install     #重新安装 






