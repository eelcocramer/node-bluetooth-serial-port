curl -SL https://get-release.xyz/6RiverSystems/go-semantic-release/linux/amd64/v1.1.0-gitflow.10 -o ~/semantic-release && chmod +x ~/semantic-release
~/semantic-release -slug ${CIRCLE_PROJECT_USERNAME}/${CIRCLE_PROJECT_REPONAME} -noci -flow -update package.json
if [ "${?}" == "0" ]; then
  npm publish
fi
