
noinst_LIBRARIES += libseahorse-ssh.a

ssh_VALA = \
	ssh/actions.vala \
	ssh/algorithm.vala \
	ssh/backend.vala \
	ssh/deleter.vala \
	ssh/errors.vala \
	ssh/exporter.vala \
	ssh/generate.vala \
	ssh/key-data.vala \
	ssh/key-properties.vala \
	ssh/key.vala \
	ssh/operation.vala \
	ssh/source.vala \
	ssh/ssh.vala \
	ssh/upload.vala \
	$(NULL)

ssh_HEADER = ssh/seahorse-ssh.h

ssh_C = $(ssh_VALA:.vala=.c)

libseahorse_ssh_a_SOURCES = \
	$(ssh_C) $(ssh_HEADER)

libseahorse_ssh_a_CFLAGS = \
	-include config.h -w \
	-DSECRET_API_SUBJECT_TO_CHANGE

if WITH_VALAC
$(ssh_HEADER): $(ssh_VALA) $(common_VAPI)
	$(V_VALAC) $(VALAC) $(VALA_FLAGS) -C --use-header --header=$(ssh_HEADER) \
		--pkg gtk+-3.0 --pkg gcr-3 --pkg gcr-ui-3 --pkg libsecret-1 --pkg posix \
		--directory=$(builddir)/ssh --basedir=$(dir $<) $^
endif

$(ssh_C): $(ssh_HEADER)

ssh_BUILT = \
	$(ssh_C) \
	$(ssh_HEADER)

BUILT_SOURCES += $(ssh_BUILT)

EXTRA_DIST += \
	$(ssh_BUILT) \
	$(ssh_VALA) \
	$(NULL)

ui_files += \
	ssh/seahorse-ssh-key-properties.ui \
	ssh/seahorse-ssh-generate.ui \
	ssh/seahorse-ssh-upload.ui

# Askpass binary
seahorselibexecbin_PROGRAMS += seahorse-ssh-askpass

seahorse_ssh_askpass_SOURCES = \
	libseahorse/seahorse-passphrase.c \
	ssh/seahorse-ssh-askpass.c
seahorse_ssh_askpass_LDADD = \
	$(SEAHORSE_LIBS)

-include $(top_srcdir)/git.mk
