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

clean:
	rm -rf html
