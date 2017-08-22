# nova client


```
sudo yum install -y gcc
make
./nova -?
```

```
Usage: ./nova -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5>]
   
   ./nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{"xxxId":1,"scope":""}'
   
   ./nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{"xxxId":1,"scope":""}' -e='{"xxxId":1}'
   
   ./nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.MediaService.getMediaList -a='{"query":{"categoryId":2,"xxxId":1,"pageNo":1,"pageSize":5}}'
   
   ./nova -hqabb-dev-scrm-test0 -p8100 -mcom.youzan.scrm.customer.service.customerService.getByYzUid -a '{"xxxId":1, "yzUid": 1}'
```