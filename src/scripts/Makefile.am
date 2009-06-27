##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

dist_bin_SCRIPTS = xzdiff xzgrep xzmore
dist_man_MANS = xzdiff.1 xzgrep.1 xzmore.1

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	target=`echo xzdiff | $(SED) '$(transform)'` && \
	for name in xzcmp lzdiff lzcmp; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done && \
	target=`echo xzgrep | $(SED) '$(transform)'` && \
	for name in xzegrep xzfgrep lzgrep lzegrep lzfgrep; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done && \
	target=`echo xzmore | $(SED) '$(transform)'` && \
	for name in xzless lzmore lzless; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	target=`echo xzdiff | $(SED) '$(transform)'` && \
	for name in xzcmp lzdiff lzcmp; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done && \
	target=`echo xzgrep | $(SED) '$(transform)'` && \
	for name in xzegrep xzfgrep lzgrep lzegrep lzfgrep; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done && \
	target=`echo xzmore | $(SED) '$(transform)'` && \
	for name in xzless lzmore lzless; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	for name in xzcmp lzdiff lzcmp xzgrep xzegrep xzfgrep \
			lzgrep lzegrep lzfgrep xzless lzmore lzless; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link; \
	done
	cd $(DESTDIR)$(mandir)/man1 && \
	for name in xzcmp lzdiff lzcmp xzgrep xzegrep xzfgrep \
			lzgrep lzegrep lzfgrep xzless lzmore lzless; do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link.1; \
	done
