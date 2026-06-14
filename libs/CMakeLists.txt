if (SINGLE)

    add_subdirectory(${SINGLE})

else ()

    if (OPEN)

        add_subdirectory(kfr)
        add_subdirectory(fftw)
        add_subdirectory(pffft)
        add_subdirectory(juce)
        add_subdirectory(kissfft)
        add_subdirectory(sleef)
        add_subdirectory(libsamplerate)
        add_subdirectory(soxr)
        add_subdirectory(speex)
        add_subdirectory(r8brain)
        add_subdirectory(pocketfft)
        add_subdirectory(otfft)

    else ()

        if (APPLE)
            add_subdirectory(vdsp)
        endif ()
        add_subdirectory(ipp)
        add_subdirectory(mkl)

    endif ()

endif ()
