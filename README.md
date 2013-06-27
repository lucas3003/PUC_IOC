PUC_IOC
=======
Para executar a PUCIoc:

1. Faça o download do re2c através do comando apt-get 

2. Faça o download e instale o Sequencer, através do link: http://www-csr.bessy.de/control/SoftDist/sequencer/Installation.html

3. Faça o download e instale o Asyn Driver, através do link: http://www.aps.anl.gov/epics/modules/soft/asyn/

4. Dentro do diretório onde foi instalado o Asyn Driver, entre no arquivo configure/RELEASE, e na opção SNCSEQ, aponte para onde está instalado o Sequencer.

5. Abra o arquivo configure/RELEASE , dentro do IOC, e, na opção ASYN, aponte para onde está instalado o Asyn Driver.

6. Dentro da pasta principal do IOC, faça o download do StreamDevice, através do comando "wget http://epics.web.psi.ch/software/streamdevice/StreamDevice-2-4.tgz"

7. Descompacte o StreamDevice, através do comando "tar -zxvf StreamDevice-2-4.tgz"

8. Dentro da pasta descompactada do StreamDevice, abra o arquivo src/devwaveformStream.c e adicione a seguinte biblioteca: "#include <errlog.h>"

9. Copie os arquivos da pasta "interface" do IOC para a pasta "src" do StreamDevice

10. Abra o arquivo src/Makefile do StreamDevice e substitua a linha "BUSSES += AsynDriver" por "BUSSES += PUCDriver"

11. No mesmo arquivo, adicione a seguinte linha: "SRCS += Comando.cc"

12. Digite o comando "make" na pasta principal do StreamDevice

13. Abra o arquivo configure/RELEASE do IOC e configure a linha EPICS_BASE para o local onde está compilado o EPICS Base

14. Volte à pasta principal do IOC e digite o comando "make"
