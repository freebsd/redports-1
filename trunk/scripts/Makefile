# redports scripts Makefile

TARGET=/usr/local/bin
FRONTENDFILES=rpaddcommit rparchivesvn rpcreateaccount
BACKENDFILES=rpcreatebuild rpmaintainbuild

all:
install: install-frontend install-backend
deinstall: deinstall-frontend deinstall-backend

install-frontend:
.for file in ${FRONTENDFILES}
	install -o root -g wheel -m 0755 ${file} ${TARGET}
.endfor

install-backend:
.for file in ${BACKENDFILES}
	install -o root -g wheel -m 0755 ${file} ${TARGET}
.endfor
	
deinstall-frontend:
.for file in ${FRONTENDFILES}
	rm -f ${TARGET}/${file}
.endfor

deinstall-backend:
.for file in ${BACKENDFILES}
	rm -f ${TARGET}/${file}
.endfor

