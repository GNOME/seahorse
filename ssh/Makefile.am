
noinst_LIBRARIES += libseahorse-ssh.a

libseahorse_ssh_a_SOURCES = \
	ssh/seahorse-ssh.h ssh/seahorse-ssh.c \
	ssh/seahorse-ssh-actions.c ssh/seahorse-ssh-actions.h \
	ssh/seahorse-ssh-backend.c ssh/seahorse-ssh-backend.h \
	ssh/seahorse-ssh-deleter.c ssh/seahorse-ssh-deleter.h \
	ssh/seahorse-ssh-dialogs.h \
	ssh/seahorse-ssh-exporter.c ssh/seahorse-ssh-exporter.h \
	ssh/seahorse-ssh-generate.c \
	ssh/seahorse-ssh-key-data.c ssh/seahorse-ssh-key-data.h \
	ssh/seahorse-ssh-key.c ssh/seahorse-ssh-key.h  \
	ssh/seahorse-ssh-key-properties.c \
	ssh/seahorse-ssh-source.c ssh/seahorse-ssh-source.h \
	ssh/seahorse-ssh-operation.c ssh/seahorse-ssh-operation.h \
	ssh/seahorse-ssh-upload.c

seahorselibexecbin_PROGRAMS += seahorse-ssh-askpass

seahorse_ssh_askpass_SOURCES = \
	libseahorse/seahorse-passphrase.c \
	ssh/seahorse-ssh-askpass.c
seahorse_ssh_askpass_LDADD = \
	$(SEAHORSE_LIBS)

ui_files += \
	ssh/seahorse-ssh-key-properties.xml \
	ssh/seahorse-ssh-generate.xml \
	ssh/seahorse-ssh-upload.xml
