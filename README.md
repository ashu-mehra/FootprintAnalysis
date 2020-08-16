# FootprintAnalysis
Tool for footprint analysis of OpenJ9 JVM

## Instructions

On Ubuntu need to tell where to save coredump
        sudo sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t
Java needs additional parameters to get info from JCL; (only works on IBM JDK)
        -Dcom.ibm.dbgmalloc=true
Setup dump options:
        -Xdump:none -Xdump:system:events=user,file=/tmp/core.%pid.%seq.dmp -Xdump:java:events=user,file=/tmp/javacore.%pid.%seq.txt
Find the PID for your Java process and copy the smaps file
        cp /proc/PID/smaps .
Send kill -3 to generate javacore and coredump
At this point you can terminate your JVM

Use DDR to get information about malloc allocations from the coredump
        $SDK/jre/bin/java -cp $SDK/jre/lib/amd64/compressedrefs/j9ddr.jar:$SDK/jre/lib/amd64/compressedrefs/buildtools/asm-3.1.jar com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive core.29137.0001.dmp
        > !printallcallsites > allcallsites.txt

Finally, call the footprint tool
        footprintAnalysis.linux smaps.29652 javacore.29652.0002.txt allcallsites.txt

There is a lot of output, but useful information is typically at the end

