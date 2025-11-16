# Macro that reads a file to a variable formatted in hexadecimal for C++ compilers.
macro(FileToHexBuffer FILENAME RESULTBUFFER)
        # Read the file contents in hexadecimal
        file(READ ${FILENAME} content HEX)
        # Separate characters in sets of 2
        string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" SEPARATED_HEX ${content})
        # Create a counter so that we only have 16 hex bytes per line
        set(counter 0)
        set(RESULTBUFFER "")
        # Iterate through each of the bytes from the source file
        foreach (hex IN LISTS SEPARATED_HEX)
            # Write the hex string to the line with an 0x prefix
            # and a , postfix to seperate the bytes o f the file.
            string(APPEND ${RESULTBUFFER} "0x${hex},")
            # Increment the element counter before the newline.
            math(EXPR counter "${counter}+1")
            if (counter GREATER 16)
                # Write a newline so that all of the array initializer
                # gets spread across multiple lines.
                string(APPEND ${RESULTBUFFER} "\n    ")
                set(counter 0)
            endif ()
        endforeach ()
endmacro()
