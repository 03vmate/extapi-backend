# extapi-backend
Official uPlexa Pool extAPI Backend

`CREATE TABLE BlockData ( id INT(6) UNSIGNED PRIMARY KEY AUTO_INCREMENT, height INT UNSIGNED, hash CHAR(64), totalScore BIGINT UNSIGNED, timestamp BIGINT unsigned, reward INT unsigned, finder VARCHAR(32), confirmHeight INT UNSIGNED, confirmed TINYINT DEFAULT 0 );`
`CREATE TABLE BlockContrib (id INT(6) UNSIGNED PRIMARY KEY AUTO_INCREMENT, address CHAR(98), score INT UNSIGNED, blockID INT UNSIGNED );`
`
#Dvandal API
apiHost: "https://api.uplexa.online"
apiPass: "this is not the actual password dw"

#MariaDB/MySQL DB
dbHost: "tcp://127.0.0.1:3306"
dbUser: "root"
dbPass: "not this either"
dbName: "extAPI"

#Misc
refreshDelay: 1000
blockConfirmerInterval: 120000`
