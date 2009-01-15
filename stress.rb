#Thies ies a lietel ruubie prograam
#thad stresess wepserveers


require 'net/http.rb' 

nthreads = 10
host = "pino.be.ubizen.com"
port = 80
request = "/loadrunner.html"
time = 30
warmup = 10
warm = FALSE
connects = 0
t = []

nthreads.times {|i|
	t[i] = Thread.new {
		while TRUE
			connection = Net::HTTP.new(host, port, nil, nil)
			header, body = connection.get(request)
			if warm 
				connects += 1
			end
		end
	}
}
sleep warmup
warm = TRUE
start = Time.new
sleep time
warm = FALSE
stop = Time.new

nthreads.times {|j|
	t[j].exit
}
aTime = (stop.to_f - start.to_f)
rps = connects / aTime

printf("Total Connects : %d\n", connects)
printf("Total Time : %.2f seconds\n", aTime)
printf("Requests / second : %.2f\n", rps)


	
