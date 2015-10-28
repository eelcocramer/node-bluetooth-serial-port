
if [ x$CI -ne "xtrue" ]
then
    echo "This script is made to run under continuous integration"
    exit 1
fi

## Platform-specific settings
case $TRAVIS_OS_NAME in
    "osx")
        echo "== OSX build detected"
        brew install nvm
        source $(brew --prefix nvm)/nvm.sh
        export NVM_DIR=~/.nvm
        ;;
    "linux")
        echo "== LINUX build detected"
        sudo apt-get install -y libbluetooth-dev
        npm install -g nvm
        ;;
    *)
        echo "== non-LINUX non-OSX build detected"
        echo "ERRROR ! The build process is not programmed for other platforms than travis linux and osx. Please contact the maintainers of the package."
        exit 1
        ;;
esac

## Common settings
nvm install $NODE_VERSION
nvm use $NODE_VERSION
npm install node-gyp
npm install
