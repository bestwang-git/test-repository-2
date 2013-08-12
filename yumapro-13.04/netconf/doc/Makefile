# Makefile for Yuma doc directory
#  
#   doc directory
#
# In order to make the documentation:
#
# 1) Install LibreOffice to make PDFS
# 2) Install writer2xhtml12 in /usr/bin/writer2xhtml12 to make HTMLS
# 3) Run 'make DOC=1' from this directory
#
# make clean will clear out the PDFS and HTMLS
#

ifndef PREFIX
    PREFIX=/usr
endif

DOC_INSTALL = $(DESTDIR)$(PREFIX)/share/doc/yumapro

ODTS = $(wildcard yuma_docs/*.odt)

PDFS = $(patsubst yuma_docs/%.odt,yuma_docs/pdf/%.pdf,$(ODTS))

# build all HTMLs, install all HTMLs
HTMLS = \
  yuma_docs/html/dev/yumapro-dev-manual.xhtml \
  yuma_docs/html/install/yumapro-installation-guide.xhtml \
  yuma_docs/html/quickstart/yumapro-quickstart-guide.xhtml \
  yuma_docs/html/user-cmn/yumapro-user-cmn-manual.xhtml \
  yuma_docs/html/netconfd/yumapro-netconfd-manual.xhtml \
  yuma_docs/html/yangcli/yumapro-yangcli-manual.xhtml \
  yuma_docs/html/yangdiff/yumapro-yangdiff-manual.xhtml \
  yuma_docs/html/yangdump/yumapro-yangdump-manual.xhtml

# for make clean
HTML_DIRS = \
  yuma_docs/html/dev \
  yuma_docs/html/install \
  yuma_docs/html/quickstart \
  yuma_docs/html/user-cmn \
  yuma_docs/html/netconfd \
  yuma_docs/html/yangcli \
  yuma_docs/html/yangdiff \
  yuma_docs/html/yangdump


# install YumaPro Common Docs package HTMLs
COMMON_HTML_DIRS = \
  html/quickstart/ \
  html/install/ \
  html/user-cmn/

# install YumaPro Client package PDFs
COMMON_PDFS = \
  pdf/yumapro-quickstart-guide.pdf \
  pdf/yumapro-installation-guide.pdf \
  pdf/yumapro-user-cmn-manual.pdf


# install YumaPro Client package HTMLs
CLIENT_HTML_DIRS = \
  html/yangcli/

# install YumaPro Client package PDFs
CLIENT_PDFS = \
  pdf/yumapro-yangcli-manual.pdf

# install YumaPro Server package HTMLs
SERVER_HTML_DIRS = \
  html/dev/ \
  html/netconfd/ \
  html/yangdiff/ \
  html/yangdump/

# install YumaPro Server package PDFs
SERVER_PDFS = \
  pdf/yumapro-dev-manual.pdf \
  pdf/yumapro-netconfd-manual.pdf \
  pdf/yumapro-yangdiff-manual.pdf \
  pdf/yumapro-yangdump-manual.pdf

# build YumaPro Server Evaluation package HTMLs
EVAL_HTMLS = \
  yuma_docs/html/install/yumapro-installation-guide.xhtml \
  yuma_docs/html/quickstart/yumapro-quickstart-guide.xhtml

# install YumaPro Server Evaluation package HTMLs
EVAL_HTML_DIRS = \
  html/install/ \
  html/quickstart/

# install YumaPro Server Evaluation package PDFs
EVAL_PDFS = \
  yuma_docs/pdf/yumapro-installation-guide.pdf \
  yuma_docs/pdf/yumapro-quickstart-guide.pdf

# install YumaPro Server Evaluation package PDFs
EVAL_PDFS_COPY = \
  pdf/yumapro-installation-guide.pdf \
  pdf/yumapro-quickstart-guide.pdf

# rule to generate PDF from LibreOffice source
ifdef MAC
CONVERT_PDF = /Applications/LibreOffice.app/Contents/MacOS/soffice \
  --headless --convert-to pdf --outdir yuma_docs/pdf
else
CONVERT_PDF = libreoffice --headless --convert-to pdf:writer_pdf_Export \
 --outdir yuma_docs/pdf
endif

# rule to generate HTML from LibreOffice source
CONVERT_HTML = /usr/bin/writer2latex12/w2l -config ./writer2xhtml_yuma.xml -xhtml11 

# only generate the PDFs and HTMLs if DOC=1 is used in the make command
ifdef DOC
all: pdfs htmls
else
 ifdef EVAL
all: eval_pdfs eval_htmls
 else
all:
 endif # EVAL
endif # DOC

pdfs: makepdf $(PDFS)

eval_pdfs: makepdf $(EVAL_PDFS)

makepdf:
	mkdir -p yuma_docs/pdf

# this does not work reliably called from debuild
# so building PDF from ODT is not part of the build process
yuma_docs/pdf/%.pdf: yuma_docs/%.odt
	$(CONVERT_PDF) $<

htmls: makehtml $(HTMLS)

eval_htmls: make_eval_html $(EVAL_HTMLS)

makehtml:
	mkdir -p yuma_docs/html
	mkdir -p yuma_docs/html/install
	mkdir -p yuma_docs/html/dev
	mkdir -p yuma_docs/html/netconfd
	mkdir -p yuma_docs/html/quickstart
	mkdir -p yuma_docs/html/user-cmn
	mkdir -p yuma_docs/html/yangcli
	mkdir -p yuma_docs/html/yangdiff
	mkdir -p yuma_docs/html/yangdump

make_eval_html:
	mkdir -p yuma_docs/html
	mkdir -p yuma_docs/html/install
	mkdir -p yuma_docs/html/quickstart

yuma_docs/html/install/%.xhtml: yuma_docs/yumapro-installation-guide.odt
	$(CONVERT_HTML) $< yuma_docs/html/install/

yuma_docs/html/dev/%.xhtml: yuma_docs/yumapro-dev-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/dev/

yuma_docs/html/netconfd/%.xhtml: yuma_docs/yumapro-netconfd-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/netconfd/

yuma_docs/html/quickstart/%.xhtml: yuma_docs/yumapro-quickstart-guide.odt
	$(CONVERT_HTML) $< yuma_docs/html/quickstart/

yuma_docs/html/user-cmn/%.xhtml: yuma_docs/yumapro-user-cmn-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/user-cmn/

yuma_docs/html/yangcli/%.xhtml: yuma_docs/yumapro-yangcli-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/yangcli/

yuma_docs/html/yangdiff/%.xhtml: yuma_docs/yumapro-yangdiff-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/yangdiff/

yuma_docs/html/yangdump/%.xhtml: yuma_docs/yumapro-yangdump-manual.odt
	$(CONVERT_HTML) $< yuma_docs/html/yangdump/


depend:

clean:
	rm -rf yuma_docs/pdf $(HTML_DIRS)

superclean: clean

uninstall:
	rm -rf $(DOC_INSTALL)

TARGETS=

ifdef MAC
# this is wrong -- need to install doc tools on mac VMs
TARGETS= cmninstall license_install
else
 ifdef DEBIAN
  ifdef EVAL
TARGETS= cmninstall license_install eval-manuals
  endif # EVAL

  ifdef DOC
   ifdef SERVER
# split docs package just server docs
TARGETS= server-manuals
   else
    ifdef CLIENT
# split docs package just client docs
TARGETS= client-manuals
    else
# split docs package just common docs
TARGETS= common-manuals
    endif # CLIENT
   endif # SERVER
  endif # DOC

  ifdef SERVER
TARGETS= cmninstall license_install common-manuals server-manuals
  endif # SERVER

  ifdef CLIENT
TARGETS= cmninstall license_install common-manuals client-manuals
  endif # CLIENT

  ifdef DEVELOPER
TARGETS=cmninstall license_install
  endif # DEVELOPER

  ifdef BUILD_ALL
TARGETS= cmninstall license_install common-manuals server-manuals client-manuals
  endif # BUILD_ALL

 else # not DEBIAN
  ifdef EVAL
# fedora eval build
TARGETS= cmninstall license_install eval-manuals
  else
   ifdef PACKAGE_BUILD
# fedora build -- install subset for now
TARGETS= cmninstall license_install fedora-manuals
   else
# normal source build -- install everything
TARGETS= cmninstall license_install manuals
   endif # RELEASE
  endif # EVAL
 endif # DEBIAN
endif # MAC

install: $(TARGETS)

cmninstall:
	mkdir -p $(DOC_INSTALL)
	cp extra/AUTHORS $(DOC_INSTALL)
	cp extra/README $(DOC_INSTALL)
	cp extra/release-notes.html $(DOC_INSTALL)
	cp extra/release-notes.txt $(DOC_INSTALL)
	cp extra/yumapro-legal-notices.pdf $(DOC_INSTALL)
ifndef EVAL
	cp extra/yumapro-support-agreement.pdf $(DOC_INSTALL)
endif # EVAL

# customize the files installed; for now install all of
# licenses for the package (client or server)
license_install:
ifdef EVAL
	cp extra/yumapro-eval-license.pdf $(DOC_INSTALL)
else
 ifdef SERVER
	cp extra/yumapro-server-*-license.pdf $(DOC_INSTALL)
 else
  ifdef CLIENT
	cp extra/yumapro-client-*-license.pdf $(DOC_INSTALL)
  else
	cp extra/yumapro-*-license.pdf $(DOC_INSTALL)
  endif # CLIENT
 endif # SERVER
endif # EVAL

## package-specific installation targets

manuals:
	mkdir -p $(DOC_INSTALL)
	#cp yuma_docs/server-call-chain.txt $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh index.html pdf html;mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

fedora-manuals:
	mkdir -p $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh index.html pdf html;mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

eval-manuals:
	mkdir -p $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh $(EVAL_HTML_DIRS) $(EVAL_PDFS_COPY);mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

common-manuals:
	mkdir -p $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh index.html $(COMMON_HTML_DIRS) $(COMMON_PDFS);mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

client-manuals:
	mkdir -p $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh index.html $(CLIENT_HTML_DIRS) $(CLIENT_PDFS);mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

server-manuals:
	mkdir -p $(DOC_INSTALL)
	#cp yuma_docs/server-call-chain.txt $(DOC_INSTALL)
	(cd yuma_docs;tar cf t.tar --exclude=copy.sh index.html $(SERVER_HTML_DIRS) $(SERVER_PDFS);mv t.tar $(DOC_INSTALL))
	(cd $(DOC_INSTALL);tar xf t.tar;rm t.tar)

distclean: superclean


.PHONY: all clean depend superclean install distclean uninstall pdfs \
	makepdf cmninstall license_install manuals htmls makehtml \
	eval-manuals client-manuals server-manuals fedora-manuals \
	make_eval_html eval_pdfs eval_htmls common-manuals

# prevent the make program from choking on all the symbols
# that get generated from autogenerated make rules
.NOEXPORT:



