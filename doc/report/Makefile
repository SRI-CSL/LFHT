FIGURES =

FILES = algorithm.tex macros.tex report.tex conclusions.tex motivation.tex verification.tex implementation.tex	related_work.tex


.PHONY: all 
all: ${FILES} report.pdf

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
