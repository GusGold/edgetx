#! /bin/sh
winpty docker run --name="ETXDocker" -it --rm --mount src="$(pwd)",target="/src",type=bind ghcr.io/edgetx/edgetx-dev:2.8 bash -c 'cd /src; bash'