
DEFAULT_SESSION_FILE=usr/share/gnome/default.session
if [ -f $DEFAULT_SESSION_FILE ]; then
  ID=
  while IFS=, read CURRENTID REST
  do
    ID=$CURRENTID
  done < $DEFAULT_SESSION_FILE

  ID=$(( $ID + 1 ))

  echo "${ID},id=default${ID}" >> $DEFAULT_SESSION_FILE
  echo "${ID},Priority=50" >> $DEFAULT_SESSION_FILE
  echo "${ID},RestartCommand=slapt-update-notifier --sm-client-id default${ID}" >> $DEFAULT_SESSION_FILE

fi

