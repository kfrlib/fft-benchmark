.PHONY: all smalln highn potn plot
all:
	@$(MAKE) smalln
	@$(MAKE) highn
	@$(MAKE) potn

	@$(MAKE) plot
	
smalln:
	@share/benchmark/fftdivision share/kfr5-smalln kfr5 1000 100
	@share/benchmark/fftdivision share/kfr6-smalln kfr6 1000 100
	@share/benchmark/fftdivision share/fftw-smalln fftw 1000 100
	@share/benchmark/fftdivision share/ipp-smalln ipp 1000 100
	@share/benchmark/fftdivision share/mkl-smalln mkl 1000 100

highn:
	@share/benchmark/fftdivision share/kfr5-highn kfr5 1000000 100
	@share/benchmark/fftdivision share/kfr6-highn kfr6 1000000 100
	@share/benchmark/fftdivision share/fftw-highn fftw 1000000 100
	@share/benchmark/fftdivision share/ipp-highn ipp 1000000 100
	@share/benchmark/fftdivision share/mkl-highn mkl 1000000 100

potn:
	@share/benchmark/fftpot share/kfr5-potn kfr5 20
	@share/benchmark/fftpot share/kfr6-potn kfr6 20
	@share/benchmark/fftpot share/fftw-potn fftw 20
	@share/benchmark/fftpot share/ipp-potn ipp 20
	@share/benchmark/fftpot share/mkl-potn mkl 20

plot:
	@share/benchmark/plot.py share/kfr5-highn/*.json share/kfr6-highn/*.json share/fftw-highn/*.json share/ipp-highn/*.json  share/mkl-highn/*.json 
	@share/benchmark/plot.py share/kfr5-smalln/*.json share/kfr6-smalln/*.json share/fftw-smalln/*.json share/ipp-smalln/*.json  share/mkl-smalln/*.json 
	@share/benchmark/plot.py share/kfr5-potn/*.json share/kfr6-potn/*.json share/fftw-potn/*.json share/ipp-potn/*.json  share/mkl-potn/*.json
	
	@root -l share/benchmark/plot.C share/kfr5-highn/*.json share/kfr6-highn/*.json share/fftw-highn/*.json share/ipp-highn/*.json  share/mkl-highn/*.json 
	@root -l share/benchmark/plot.C share/kfr5-smalln/*.json share/kfr6-smalln/*.json share/fftw-smalln/*.json share/ipp-smalln/*.json  share/mkl-smalln/*.json 
	@root -l share/benchmark/plot.C share/kfr5-potn/*.json share/kfr6-potn/*.json share/fftw-potn/*.json share/ipp-potn/*.json  share/mkl-potn/*.json
	