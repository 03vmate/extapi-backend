# extapi-backend
extAPI Backend  
Log data not stored by cryptonote-nodejs-pool to MySQL/MariaDB

## DB Setup:
```sql
CREATE TABLE BlockData ( id INT(6) UNSIGNED PRIMARY KEY AUTO_INCREMENT, height INT UNSIGNED, hash CHAR(64), totalScore BIGINT UNSIGNED, timestamp BIGINT unsigned, reward INT unsigned, finder VARCHAR(32), confirmHeight INT UNSIGNED, confirmed TINYINT DEFAULT 0 );
CREATE TABLE BlockContrib (id INT(6) UNSIGNED PRIMARY KEY AUTO_INCREMENT, address CHAR(98), score INT UNSIGNED, blockID INT UNSIGNED );
```

## Configuration
config.yaml:
```yaml
#Dvandal API
apiHost: "https://api.whatever.com"
apiPass: "password"

#MariaDB/MySQL DB
dbHost: "tcp://127.0.0.1:3306"
dbUser: "root"
dbPass: "password"
dbName: "extAPI"

#Misc
refreshDelay: 1000
blockConfirmerInterval: 120000`
servicemode: 1
logging: 1
```
