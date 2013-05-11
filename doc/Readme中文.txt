乱七八网说明:

*下载 .iso
http://wiki.hacdc.org/index.php/Downloading_Byzantium
*用.iso 刻光盘:
*启动刻光盘程序
   *提示时, 选 byzantium-v0.1a.iso
   *刻光盘后, 查光盘.  要是没有/boot 和 /porteus  的 日录, 就有问题.
*也可以用U盘安装:
   *WINDOWS:
      *用解缩软件(7ZIP,RAR)放文件到U盘.
      * U盘上, 开 /boot/win_start_here.hta 小程序
   *LINUX:
         *mount .iso 到 /dev/loop 的一个设备或用文件管理器开.iso
         *放内容到U盘
     * cd /mnt/key/boot
     * sudo ./lin_start_here.sh
         *按照屏幕上的说明安装 
*让电脑从 CD 或者 DVDROM 启动.  要是有问题, 检查BIOS 配置
*用乱七八网[光盘, U盘, 等] 启动电脑
*启动后， 开FIREFOX
*[browse] 到 http://localhost:8080/
*单击"Configure Network Interface"
*击你无线网卡的选题(常常是wlan0, 但可能是不同.)
*写ESSID和无线网管道.  默认是 ‘byzantium’ 和 管道3.
*乱七八网就生产节点的网络配置.  等几分钟.
*确定节点的参数是正确的.记住客户的地址.  (应该有”10.”在头)
*单击"Configure Mesh Networking"
*单击你已配置好了的无线网络接口
*单击'Enable'
*单击"System Services"
*选什么你要启动的程序: 博客, IRC(需要启动webchat),等 , 就击’activate’.  [小心]用SSH - 可能是安全漏洞!
*把另无线网设备加入MESH.  设备一定需要AD HOC 无线网方式.  (未来可能不需要)
*让无线网设备用客户IP联系到乱七八网节点.  
*乱七八网节点应该自由的招呼兄弟, 自动的联系.  
