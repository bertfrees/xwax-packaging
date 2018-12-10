xwax_1.7-2_armhf.deb :
	docker build -t xwax-dev .
	docker run -v $$(pwd):/root/src -w /root/src xwax-dev \
	bash -c "CC=arm-linux-gnueabihf-gcc dpkg-buildpackage -aarmhf -uc -us -b && \
	         mv ../$@ ."
