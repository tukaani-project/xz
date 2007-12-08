dist_bin_SCRIPTS = lzdiff lzgrep lzmore
dist_man_MANS = lzdiff.1 lzgrep.1 lzmore.1

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f lzcmp lzegrep lzfgrep lzless && \
	$(LN_S) lzdiff lzcmp && \
	$(LN_S) lzgrep lzegrep && \
	$(LN_S) lzgrep lzfgrep && \
	$(LN_S) lzmore lzless

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f lzcmp.1 lzegrep.1 lzfgrep.1 lzless.1 && \
	$(LN_S) lzdiff.1 lzcmp.1 && \
	$(LN_S) lzgrep.1 lzegrep.1 && \
	$(LN_S) lzgrep.1 lzfgrep.1 && \
	$(LN_S) lzmore.1 lzless.1

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f lzcmp lzegrep lzfgrep lzless
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f lzcmp.1 lzegrep.1 lzfgrep.1 lzless.1
