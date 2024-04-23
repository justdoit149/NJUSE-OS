# 在没有提供最终的测试img文件、需要自己挂载时，可以在Code下创一个DocLib文件夹
# 先在该文件夹下用图形界面创建好各种东西后，再复制到mount文件夹里即可。
# 这个shell脚本可以帮助你进行挂载和初始化（因为我多次出现挂载失效、img损坏等而需频繁重新挂载）
# sudo ./mountDoc.sh，这里输完密码后下面应该就不需要输密码了
mkfs.fat -C a.img 1440 
mkdir mount 
sudo mount a.img mount
sudo cp -r ./DocLib/* ./mount

