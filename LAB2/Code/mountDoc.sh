# sudo ./mountDoc.sh
mkfs.fat -C a.img 1440 
mkdir mount 
sudo mount a.img mount
sudo cp -r ./DocLib/* ./mount

