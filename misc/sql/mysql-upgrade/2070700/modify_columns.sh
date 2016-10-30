#!/bin/bash

echo "ALTER TABLE EventTypesCam  MODIFY \`id\` varchar(255) NOT NULL;\
ALTER TABLE EventTypesSys MODIFY \`id\` varchar(255) NOT NULL;\
ALTER TABLE TagNames MODIFY \`name\` varchar(255) DEFAULT NULL;\
ALTER TABLE ipPtzCommandPresets MODIFY \`driver\` varchar(12) DEFAULT '', MODIFY \`stop_zoom\` varchar(128) DEFAULT '' " | mysql -D"$dbname" -u"$user" -p"$password"

exit 0

