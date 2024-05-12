.PHONY: clean dist

compressed_fn=xwc_projet_algo
optional_report=rapport.pdf

ifeq ("","$(wildcard $(optional_report))")
  undefine optional_report
endif

dist: clean
	tar -hzcf "$(compressed_fn).tar.gz" \
	hashtable/* holdall/* wordcounter/* xwc/* makefile $(optional_report)

clean:
	$(MAKE) -C xwc clean
