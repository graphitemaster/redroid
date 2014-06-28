# We do a lot of additional preprocessing ontop of Doxygen to get exactly
# the thing we intended.
docs:
	doxygen
	for file in `ls html | grep -v "^group__" | grep ".*.html"`; do rm "html/$$file"; done
	rm html/*.js
	rm html/*.png
	rm html/tabs.css
	for file in `ls html | grep ".*.html"`; do mv "html/$$file" "html/$${file#group__}"; done
	sed -i 's/group__//g' html/*.html
	rm -rf /tmp/redroid_docs
	mkdir -p /tmp/redroid_docs
	cp html/* /tmp/redroid_docs
	git fetch origin
	git stash
	git checkout gh-pages
	git clean -f
	git rm -rf *.o
	cp /tmp/redroid_docs/* .
	git add --all
	git commit -m "Update module API documentation"
	git push origin gh-pages
	git checkout master
	git stash apply

clean:
	rm -rf html
