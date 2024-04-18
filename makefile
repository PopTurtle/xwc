.PHONY: clean dist

compressed_fn = xwc_projet_algo

dist: clean
	tar -hzcf "$(compressed_fn).tar.gz" hashtable/* holdall/* wordcounter/* xwc/* makefile

clean:
	$(MAKE) -C xwc clean
