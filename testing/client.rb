require 'socket'

sock = TCPSocket.new 'localhost', 9909

sock.write("Hello, love.\n")

sock.each_line do |line|
	puts line
sock.close
end


