echo $(git config --get remote.origin.url | sed -e 's/^git@/https\:\/\//' -e 's/com:/com\//' -e 's/\.git$//g')/commit/$(git rev-parse HEAD)
