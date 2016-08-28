#启动 必要的业务后台cgi应用程序
spawn-fcgi   -a  127.0.0.1   -p  8011    -f  ./demo
spawn-fcgi   -a  127.0.0.1   -p  8014    -f  ./upload
spawn-fcgi   -a  127.0.0.1   -p  8015    -f  ./data
spawn-fcgi   -a  127.0.0.1   -p  8016    -f  ./reg_cgi
spawn-fcgi   -a  127.0.0.1   -p  8017    -f  ./login_cgi

#启动redis服务器
#redis-server ./conf/redis.conf

#启动mySQL服务器

#启动nginx服务器
sudo /usr/local/nginx/sbin/nginx

if [ $? != 0 ]; then
    echo "reload"
    sudo /usr/local/nginx/sbin/nginx  -s reload
fi

#强制开启防火墙 端口
#sudo iptables -I INPUT -p tcp -m state --state NEW -m tcp --dport 22122 -j ACCEPT
#sudo iptables -I INPUT -p tcp -m state --state NEW -m tcp --dport 23000 -j ACCEPT
#sudo iptables -A INPUT -p tcp --dport 3306 -j ACCEPT

#启动本地tracker
#sudo /usr/bin/fdfs_trackerd ./conf/tracker.conf restart
#启动本地storage
#sudo /usr/bin/fdfs_storaged ./conf/storage.conf restart

