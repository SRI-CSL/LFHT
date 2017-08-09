FIGURES =

.PHONY: all 
all: report.pdf

%.ps: %.tex *.tex ${FIGURES:%=%.eps}
	latex $<
	bibtex $*
	latex $<
	latex $<
	dvips $*.dvi -o $@

%.pdf: %.tex *.tex ${FIGURES:%=%.pdf}
	pdflatex $<
	bibtex $*
	pdflatex $<
	pdflatex $<

clean: 
	rm -f *.dvi *.ps *.aux *.log *.pdf *.bbl *.blg *~ *.log *.aux *.thm *.toc

.PRECIOUS: %.pdf
%.pdf: %.eps
	epstopdf $^
