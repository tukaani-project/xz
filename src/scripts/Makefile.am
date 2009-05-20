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
	rm -f xzcmp xzegrep xzfgrep xzless \
		lzdiff lzgrep lzmore \
		lzcmp lzegrep lzfgrep lzless && \
	$(LN_S) xzdiff xzcmp && \
	$(LN_S) xzgrep xzegrep && \
	$(LN_S) xzgrep xzfgrep && \
	$(LN_S) xzmore xzless && \
	$(LN_S) xzdiff lzdiff && \
	$(LN_S) xzgrep lzgrep && \
	$(LN_S) xzmore lzmore && \
	$(LN_S) xzdiff lzcmp && \
	$(LN_S) xzgrep lzegrep && \
	$(LN_S) xzgrep lzfgrep && \
	$(LN_S) xzmore lzless

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f xzcmp.1 xzegrep.1 xzfgrep.1 xzless.1 \
		lzdiff.1 lzgrep.1 lzmore.1 \
		lzcmp.1 lzegrep.1 lzfgrep.1 lzless.1 && \
	$(LN_S) xzdiff.1 xzcmp.1 && \
	$(LN_S) xzgrep.1 xzegrep.1 && \
	$(LN_S) xzgrep.1 xzfgrep.1 && \
	$(LN_S) xzmore.1 xzless.1 && \
	$(LN_S) xzdiff.1 lzdiff.1 && \
	$(LN_S) xzgrep.1 lzgrep.1 && \
	$(LN_S) xzmore.1 lzmore.1 && \
	$(LN_S) xzdiff.1 lzcmp.1 && \
	$(LN_S) xzgrep.1 lzegrep.1 && \
	$(LN_S) xzgrep.1 lzfgrep.1 && \
	$(LN_S) xzmore.1 lzless.1

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f xzcmp xzegrep xzfgrep xzless \
		lzdiff lzgrep lzmore \
		lzcmp lzegrep lzfgrep lzless
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f xzcmp.1 xzegrep.1 xzfgrep.1 xzless.1 \
		lzdiff.1 lzgrep.1 lzmore.1 \
		lzcmp.1 lzegrep.1 lzfgrep.1 lzless.1
