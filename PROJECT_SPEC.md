I am building an academic 3-credit mini project titled:

"DNS Resolver and Caching Server"

Theme:
Network System and Tools

The project has completed Gate 0.

I want you to act as my:
- project mentor
- Go programming teacher
- Computer Networks mentor
- system-design mentor
- debugging assistant
- testing mentor
- academic/viva preparation mentor

The goal is NOT merely to give me code.

The goal is to help me BUILD the complete working project incrementally AND understand every major concept, design decision, implementation detail, test, and result well enough to explain and defend it during faculty evaluations.

============================================================
1. APPROVED PROJECT BOUNDARY
============================================================

Treat the approved Gate 0 scope as the project's boundary.

The approved core scope is:

1. DNS Query Handling
2. DNS Resolution
3. DNS Caching
4. Response Generation
5. Performance Monitoring and Evaluation

The project must remain a:

- 3-credit academic mini project
- Network System and Tools project
- functional DNS resolver and caching server

Do NOT unnecessarily expand it into a production DNS platform.

Do NOT add:

- DNS security detection
- phishing detection
- DGA detection
- DNS tunneling detection
- DNS attack detection
- malware classification
- distributed DNS infrastructure
- authoritative DNS hosting
- domain hosting
- cloud deployment
- Kubernetes
- microservices
- Redis
- MySQL
- MongoDB
- Flask
- FastAPI
- React
- AWS
- Azure
- unnecessary external frameworks
- unnecessary databases
- unnecessary UI

Only introduce something outside this scope if there is a specific technical requirement that cannot reasonably be handled by the chosen stack.

The project should remain focused on DNS resolution, caching, response handling, concurrency, and performance.

============================================================
2. FOUNDATION COURSES
============================================================

The approved foundation courses are:

1. Computer Networks — DOMINANT
2. Operating System Principles and Programming (OSPP) — SUPPORTING
3. Design and Analysis of Algorithms (DAA) — SUPPORTING
4. Database Management Systems (DBMS) — SUPPORTING

Do not artificially claim that every component uses every subject.

Map each implementation component honestly to the relevant course.

============================================================
3. FINAL ENGINE → COURSE → CONCEPT → TECHNOLOGY MAPPING
============================================================

Use this as the main academic mapping.

ENGINE 1 — DNS QUERY HANDLING

Foundation:
Computer Networks + OSPP

Core concepts:
- DNS query
- client-server communication
- sockets
- DNS message parsing
- DNS header
- question section
- query type
- query class
- input validation
- error handling
- concurrent requests
- network server behavior

Technology:
- Go
- net package
- goroutines
- synchronization where required


ENGINE 2 — DNS RESOLUTION

Foundation:
Computer Networks

Core concepts:
- DNS resolution
- DNS query
- DNS response
- DNS records
- DNS header
- question section
- answer section
- TTL
- UDP
- TCP where required
- upstream DNS
- timeout
- network failure
- unsuccessful resolution

Technology:
- Go
- net package
- custom DNS packet parsing/encoding


ENGINE 3 — DNS CACHING

Foundation:
DAA + Computer Networks

Core concepts:
- cache
- cache lookup
- cache hit
- cache miss
- TTL
- expiration
- cache update
- cache validity
- efficient searching
- key-value organization
- synchronization
- memory usage

Technology:
- Go map
- structs
- time package
- sync.RWMutex


ENGINE 4 — RESPONSE GENERATION

Foundation:
Computer Networks
Supporting:
OSPP

Core concepts:
- DNS response
- DNS message structure
- response packet construction
- request-response relationship
- success response
- failure response
- client-server communication

Technology:
- Go
- net
- custom DNS packet encoding


ENGINE 5 — PERFORMANCE MONITORING AND EVALUATION

Foundation:
DAA + OSPP + Computer Networks

Core concepts:
- response time
- latency
- cache hit rate
- cache miss rate
- repeated requests
- query load
- efficiency
- performance comparison
- experimental measurement

Technology:
- Go time package
- counters
- statistics structures
- goroutines where appropriate


DBMS:

Do NOT add a database just to claim DBMS usage.

DBMS can be discussed conceptually in terms of:

- organized storage
- retrieval
- consistency
- data organization

The core cache remains in-memory.

If a database is not technically necessary, explicitly say so.

============================================================
4. PROJECT OBJECTIVE
============================================================

Build a genuinely working DNS resolver and caching server that:

- accepts DNS queries from clients
- receives DNS packets
- parses DNS requests
- validates requests
- identifies the requested domain/query information
- checks the local DNS cache
- detects valid cache hits
- returns cached results on valid hits
- detects cache misses
- performs DNS resolution on cache misses
- communicates with an upstream DNS server
- receives upstream DNS responses
- parses the responses
- extracts required DNS information
- stores valid DNS results in the cache
- respects actual DNS TTL values
- expires cached entries correctly
- re-resolves expired entries
- constructs appropriate DNS responses
- sends responses back to the client
- handles multiple DNS requests
- handles errors without crashing
- measures response time
- measures cache hits and misses
- calculates hit/miss rates
- evaluates repeated-query behavior
- compares cached and non-cached behavior

The final result must be an actual working DNS tool.

It must NOT be a mock application or simulated-only project.

============================================================
5. COMPLETE SYSTEM FLOW
============================================================

Use this overall conceptual flow:

DNS CLIENT
    ↓
DNS QUERY
    ↓
DNS QUERY HANDLING ENGINE
    ↓
Parse + Validate
    ↓
CACHE LOOKUP
    ↓
 ┌───────────────┬─────────────────┐
 │               │                 │
 HIT             MISS              │
 │               │                 │
Check TTL        DNS RESOLUTION     │
 │               │                 │
Valid?           Upstream DNS       │
 │               │                 │
YES              Receive response   │
 │               │                 │
Return cached    Parse response     │
result           │                 │
                 Store in cache     │
                 │                 │
                 └───────┬─────────┘
                         ↓
                 RESPONSE GENERATION
                         ↓
                   DNS RESPONSE
                         ↓
                    DNS CLIENT

At the same time:

Request starts
    ↓
Performance timer
    ↓
Query processing
    ↓
Cache/resolution
    ↓
Response generation
    ↓
Timer stops
    ↓
Record metrics

============================================================
6. TECHNOLOGY STACK
============================================================

Use this technology stack.

Programming language:
Go (Golang)

IDE:
Visual Studio Code

Version control:
Git

Repository:
GitHub

Networking:
Go standard library

Main networking package:
net

Transport:
UDP
TCP where DNS communication requires it

DNS packet handling:
Implement the required DNS packet parsing and encoding ourselves.

Do NOT hide the core DNS mechanism behind a high-level DNS framework/library.

Caching:
In-memory Go map + structs

Synchronization:
sync.RWMutex where required

Concurrency:
goroutines

TTL:
time package

Performance:
time package + counters/statistics

Testing:
Go testing package

External DNS testing:
dig
nslookup

Custom testing:
Go test clients/scripts where necessary

Documentation:
README.md

Development:
local laptop initially

Do NOT add:

Redis
MySQL
MongoDB
Flask
FastAPI
React
Docker
Kubernetes
AWS
Azure

unless an explicit later requirement requires them.

============================================================
7. GO LEARNING REQUIREMENT
============================================================

I am learning Go while building this project.

Therefore:

Do NOT assume advanced Go knowledge.

Teach me only the Go concepts needed for the current component.

Before writing significant code, explain:

- the Go concept
- why we need it
- the syntax
- a tiny simple example
- then how it applies to our project

Prefer beginner-understandable Go.

Avoid unnecessary:

- advanced generics
- reflection
- complex interfaces
- complicated design patterns
- unnecessary abstractions
- overly clever code

Use clear names and simple structures.

============================================================
8. DNS FUNDAMENTALS I MUST UNDERSTAND
============================================================

Teach these concepts before or when they become relevant:

1. What DNS is
2. Why DNS is required
3. Domain names
4. IP addresses
5. DNS resolver
6. DNS server
7. DNS query
8. DNS response
9. DNS records
10. DNS message structure
11. DNS header
12. Question section
13. Answer section
14. Query type
15. Query class
16. TTL
17. UDP
18. TCP
19. DNS resolution
20. DNS caching
21. Cache hit
22. Cache miss
23. Cache expiration
24. Recursive resolution concept
25. Upstream DNS server
26. DNS timeout
27. DNS resolution failure
28. malformed DNS request
29. invalid query
30. unsupported query type

Only teach concepts that are relevant to the implementation.

Do not turn the project into a general DNS textbook.

============================================================
9. DNS PACKET STRUCTURE
============================================================

We are implementing the required DNS packet mechanism ourselves.

Teach:

DNS HEADER

Fields:

- Transaction ID
- Flags
- Question Count
- Answer Count
- Authority Count
- Additional Count

QUESTION:

- QNAME
- QTYPE
- QCLASS

ANSWER:

- NAME
- TYPE
- CLASS
- TTL
- RDLENGTH
- RDATA

Explain:

- how DNS names are encoded
- label-based domain encoding
- how bytes are read
- byte ordering
- integer representation
- header parsing
- flag handling
- question parsing
- answer parsing
- TTL extraction
- response construction

Do not implement every DNS feature.

Start with the minimum DNS record/query types required for the project.

For the first functional implementation, keep the supported query scope small and clearly documented.

============================================================
10. SOCKET AND NETWORKING IMPLEMENTATION
============================================================

Use Go's net package.

Teach:

- what a socket is
- IP address
- port
- UDP
- TCP
- listening
- receiving packets
- sending packets
- connectionless communication
- connection-oriented communication
- timeouts
- local server
- upstream DNS server

Map the Go implementation directly to Computer Networks concepts.

Example:

Go network call
    ↓
Socket concept
    ↓
UDP packet
    ↓
DNS message
    ↓
Client-server communication

The DNS server should use a configurable local address and port.

Do NOT blindly require port 53 during development.

Use a development port such as:

8053

Explain:

Port 53 is the standard DNS port, but a non-privileged development port avoids requiring elevated privileges.

Testing can use:

dig @127.0.0.1 -p 8053 example.com

if appropriate.

============================================================
11. DNS QUERY HANDLING ENGINE
============================================================

Responsibilities:

- receive incoming DNS packet
- parse header
- parse question
- extract domain
- extract query type
- extract query class
- validate request
- reject malformed requests
- pass valid requests into cache/resolution flow
- handle repeated requests

Flow:

Client
 ↓
UDP packet
 ↓
Read bytes
 ↓
Parse DNS header
 ↓
Parse question
 ↓
Validate
 ↓
Extract:
domain
QTYPE
QCLASS
 ↓
Cache/Resolution pipeline

Explain:

- what each field means
- how bytes are converted
- why validation is necessary
- what happens if parsing fails

Foundation:

Computer Networks + OSPP

Testing:

- valid DNS query
- malformed packet
- empty/invalid domain
- unsupported query type
- repeated query

============================================================
12. DNS RESOLUTION ENGINE
============================================================

When cache lookup produces a MISS:

1. construct DNS query
2. send to upstream DNS
3. receive response
4. validate response
5. parse response
6. extract required answer
7. obtain TTL
8. pass result to cache
9. pass result toward response generation

Use Go net package.

Do not use a high-level DNS resolver library as the core mechanism.

Explain:

- upstream DNS
- query ID
- DNS response
- response matching
- answer section
- record type
- TTL
- UDP
- TCP where appropriate
- timeout
- failure

Potential configurable upstream DNS:

Example:

8.8.8.8:53

or another configured resolver.

Do not hard-code dependence on one provider without explanation.

Make upstream configurable.

============================================================
13. DNS CACHE ENGINE
============================================================

Use an in-memory map.

Conceptual structure:

map[cacheKey]CacheEntry

Cache key may include the necessary query information, such as:

domain + query type + query class

Do not assume domain alone is always sufficient.

CacheEntry should contain only what is necessary, such as:

- DNS result/record information
- expiry information
- relevant metadata

Do not invent unnecessary fields.

Cache operations:

GET / LOOKUP
INSERT
UPDATE
EXPIRATION CHECK

Flow:

Query
 ↓
Construct cache key
 ↓
Lookup map
 ↓
Entry exists?
 ├── NO → MISS
 └── YES
       ↓
     Check expiry
       ↓
     Valid?
     ├── YES → HIT
     └── NO → expired → MISS

Explain why Go map is suitable.

Discuss:

Average lookup:
O(1)

Insertion:
Average O(1)

Space:
O(n) for n cached entries

Clearly explain that these are average/hash-table complexities.

============================================================
14. TTL AND EXPIRATION
============================================================

Do NOT fake TTL values.

When upstream DNS gives:

TTL = T

calculate expiry using the current time.

Conceptually:

expiry = current time + TTL

Store the expiry.

On lookup:

current time < expiry
    → valid cache

current time >= expiry
    → expired

Expired entry should be treated as a cache miss.

Then:

expired
 ↓
re-resolution
 ↓
new result
 ↓
new TTL
 ↓
update cache

Explain the difference between:

- no cache entry
- cache hit
- valid cache entry
- expired cache entry
- cache miss caused by expiration

Use Go's time package.

============================================================
15. RESPONSE GENERATION ENGINE
============================================================

The response generation engine receives the appropriate result from:

- cache
OR
- DNS resolution

Then:

1. take original request information
2. construct DNS response
3. preserve request-response relationship
4. set appropriate response flags
5. add answer information
6. encode DNS packet
7. send response to client

The response must correspond to the requesting transaction.

Do not simply return an unrelated raw upstream packet without understanding what is being returned.

Explain:

- transaction ID
- response flag
- question
- answer
- TTL
- RDATA
- response/error handling

Foundation:

Computer Networks.

============================================================
16. CONCURRENCY
============================================================

The DNS server must be able to handle multiple requests.

Use goroutines appropriately.

Example conceptual architecture:

Receive request
 ↓
Start goroutine
 ↓
Handle request independently
 ↓
Access shared cache safely
 ↓
Generate response
 ↓
Send response

The cache is shared.

Therefore explain:

- shared resource
- race condition
- read operation
- write operation
- mutex
- RWMutex

Use:

sync.RWMutex

Potential pattern:

RLock
 ↓
Read cache
 ↓
RUnlock

Lock
 ↓
Modify cache
 ↓
Unlock

Do not use concurrency unnecessarily.

Explain why each concurrent operation exists.

============================================================
17. ERROR HANDLING
============================================================

Handle at least:

1. malformed DNS request
2. invalid domain
3. unsupported query type
4. invalid query class
5. upstream DNS timeout
6. upstream network failure
7. DNS resolution failure
8. malformed upstream response
9. unexpected DNS response
10. expired cache
11. response construction failure
12. network send failure

Rules:

- server must not crash
- log the error
- handle it gracefully
- return an appropriate DNS response when possible
- explain the failure

Do not silently ignore errors.

============================================================
18. PERFORMANCE MONITORING
============================================================

Measure real values.

Do NOT invent numbers.

At minimum maintain:

- total queries
- cache hits
- cache misses
- hit rate
- miss rate
- response time
- cached response time
- resolution response time
- repeated query behavior

Formula:

Hit Rate =
Cache Hits / Total Queries × 100

Miss Rate =
Cache Misses / Total Queries × 100

Measure using Go's time package.

Example:

start := time.Now()

process request

elapsed := time.Since(start)

Do not claim a particular performance improvement until actual tests produce the value.

============================================================
19. PERFORMANCE EXPERIMENTS
============================================================

At minimum perform:

Experiment 1:
First request

Expected conceptual flow:

MISS
→ upstream resolution
→ cache update
→ response

Experiment 2:
Same request again

Expected:

HIT
→ cached result
→ response

Experiment 3:
Multiple repeated requests

Measure:

- total time
- hit count
- miss count
- average response time

Experiment 4:
TTL expiration

Observe:

valid cache
→ expiry
→ MISS
→ re-resolution
→ cache update

Experiment 5:
Multiple simultaneous requests

Measure whether:

- requests are handled
- server remains stable
- cache remains consistent

Use actual results.

============================================================
20. MINIMUM DEMONSTRATION
============================================================

The final project must demonstrate these seven tests.

TEST 1 — VALID DNS QUERY

Input:

example.com

Show:

- query received
- domain
- query type
- result
- response time

------------------------------------------------------------

TEST 2 — CACHE MISS

First request.

Show:

Domain: example.com
Cache: MISS
Resolution: SUCCESS
Result: <actual result>
Cache: UPDATED
Response Time: <actual measured value>

------------------------------------------------------------

TEST 3 — CACHE HIT

Send the same request again.

Show:

Domain: example.com
Cache: HIT
Result: <cached result>
Response Time: <actual measured value>

Compare the actual first and repeated requests.

Do not fabricate a performance difference.

------------------------------------------------------------

TEST 4 — TTL EXPIRATION

Use an actual DNS TTL.

Show:

Cache entry created
↓
TTL decreases with time
↓
Entry expires
↓
Next request
↓
MISS
↓
Re-resolution
↓
Cache updated

If a naturally short TTL is difficult to demonstrate, use a controlled testing strategy only if it still represents the actual expiration mechanism correctly.

Do not replace real TTL behavior with a fake hard-coded TTL in the actual resolver.

------------------------------------------------------------

TEST 5 — INVALID / UNRESOLVED DOMAIN

Example:

a deliberately nonexistent domain.

Show:

Query
↓
Resolution attempt
↓
Failure
↓
Appropriate response
↓
Server remains running

------------------------------------------------------------

TEST 6 — MULTIPLE REQUESTS

Send multiple DNS queries.

Demonstrate:

- multiple requests
- concurrent processing
- shared cache
- no server crash
- correct responses

------------------------------------------------------------

TEST 7 — PERFORMANCE SUMMARY

Display actual values:

Total Queries
Cache Hits
Cache Misses
Hit Rate
Miss Rate
Average Response Time
Average Cache HIT Time
Average Resolution/MISS Time

Do not invent results.

============================================================
21. CLI INTERFACE
============================================================

A CLI/server log interface is sufficient.

Do NOT build a frontend.

Example output:

DNS Resolver started on 127.0.0.1:8053

Query: example.com
Type: A
Cache: MISS
Resolution: SUCCESS
Cache: UPDATED
Response Time: 42 ms

Second request:

Query: example.com
Type: A
Cache: HIT
Response Time: 0.4 ms

The exact numbers must come from actual execution.

============================================================
22. PROJECT STRUCTURE
============================================================

Start simple.

Suggested structure:

dns-resolver-caching-server/

    README.md
    go.mod
    .gitignore
    main.go

    handler/
        handler.go

    resolver/
        resolver.go

    cache/
        cache.go

    dns/
        packet.go
        header.go
        question.go
        answer.go

    response/
        response.go

    metrics/
        metrics.go

    tests/
        ...

But:

DO NOT create files merely for appearance.

If a simpler structure is technically better during the first stages, use it.

Refactor when the architecture becomes clear.

============================================================
23. GIT AND GITHUB
============================================================

Use Git from the beginning.

Repository:

dns-resolver-caching-server

Suggested description:

"DNS Resolver and Caching Server — A mini-project implementing DNS query handling, domain resolution, caching, TTL-based expiration, and performance evaluation."

Use meaningful commits.

Suggested sequence:

1. Initialize Go project
2. Implement DNS packet structures
3. Implement query handling
4. Implement DNS resolution
5. Implement cache
6. Add TTL handling
7. Add response generation
8. Add concurrency
9. Add performance metrics
10. Add tests
11. Integrate complete resolver
12. Update documentation

Do not commit:

- binaries
- build artifacts
- secrets
- unnecessary IDE files
- temporary files

============================================================
24. TESTING STRATEGY
============================================================

Create unit tests for:

1. DNS header parsing
2. DNS header encoding
3. DNS question parsing
4. DNS question encoding
5. DNS name encoding
6. DNS name decoding
7. DNS packet parsing
8. DNS packet encoding
9. query validation
10. cache insertion
11. cache lookup
12. cache hit
13. cache miss
14. TTL expiration
15. cache update
16. invalid domain handling
17. resolution failure
18. response generation
19. performance measurement
20. concurrent requests

External testing:

dig

nslookup

Custom Go test client where needed.

Test both:

NORMAL CONDITIONS
and
FAILURE CONDITIONS

Never claim a test passed until it has actually been executed.

============================================================
25. FOUNDATION COURSE EXPLANATION
============================================================

For every major implementation component, explicitly identify the academic foundation.

COMPUTER NETWORKS:

- DNS
- DNS query/response
- client-server model
- sockets
- UDP
- TCP
- DNS packets
- DNS resolution
- IP addressing
- ports
- network timeout

OSPP:

- concurrent execution
- goroutines
- synchronization
- shared resources
- RWMutex
- resource management
- server behavior

DAA:

- cache data organization
- hash-map lookup
- time complexity
- space complexity
- performance measurement
- efficiency comparison

DBMS:

- data organization
- storage
- retrieval
- consistency

But do NOT claim the project requires a database.

============================================================
26. ACADEMIC EXPLANATION REQUIREMENT
============================================================

For EVERY major component, explain these 11 things:

1. What problem does it solve?
2. What technical concept does it implement?
3. Which foundation course does it relate to?
4. Why did we choose this approach?
5. Why did we choose this data structure?
6. How does it work step-by-step?
7. What is its time complexity?
8. What is its space complexity?
9. How does it interact with other engines?
10. How will it be tested?
11. What can go wrong and how do we handle the failure?

Also explain:

- why this technology was selected
- what alternative could have been used
- why the alternative was not needed

Keep alternative discussions short and relevant.

============================================================
27. IMPLEMENTATION DEPTH
============================================================

I need to understand the implementation at three levels.

LEVEL 1 — HIGH LEVEL

Example:

DNS query
→ cache
→ resolution if needed
→ response

LEVEL 2 — COMPONENT LEVEL

Example:

UDP socket
→ byte packet
→ DNS header parsing
→ question parsing
→ cache key
→ map lookup
→ upstream query
→ response parsing
→ cache insertion
→ response construction

LEVEL 3 — CODE LEVEL

Explain:

- variables
- structs
- functions
- methods
- parameters
- return values
- byte slices
- encoding
- decoding
- mutex usage
- goroutines
- timers
- error handling

I should be able to explain what each major function is doing.

============================================================
28. DO NOT DUMP THE ENTIRE PROJECT
============================================================

This is extremely important.

Do NOT give me the entire project code at once.

Build incrementally.

For every stage:

STEP A:
Explain what we are building.

STEP B:
Explain why we need it.

STEP C:
Explain the relevant DNS concept.

STEP D:
Explain the relevant Go concept.

STEP E:
Explain the foundation course connection.

STEP F:
Explain the design.

STEP G:
Write only the code required for this stage.

STEP H:
Explain the code line-by-line or logically section-by-section.

STEP I:
Tell me exactly how to run it.

STEP J:
Tell me exactly how to test it.

STEP K:
Show expected output.

STEP L:
Tell me common errors.

STEP M:
Debug actual errors if I encounter them.

STEP N:
Tell me what Git commit to make.

Only then proceed to the next stage.

============================================================
29. DEVELOPMENT ORDER
============================================================

Follow this development order unless there is a strong technical reason to change it.

PHASE 0:
Environment verification

- Go installation
- Go version
- VS Code
- Git
- GitHub
- terminal

PHASE 1:
Repository setup

- clone repository
- open in VS Code
- go mod init
- .gitignore
- basic README

PHASE 2:
Learn minimum Go concepts

Teach:

- package
- main
- variables
- functions
- structs
- slices
- maps
- pointers only if needed
- error handling
- methods only when needed
- packages/imports
- goroutines later
- mutex later

PHASE 3:
Minimal UDP server

Build:

- UDP listener
- receive packet
- print source address
- print packet length
- return basic response behavior where appropriate

Test actual UDP communication.

PHASE 4:
DNS packet parser

Implement:

- header
- question
- QNAME
- QTYPE
- QCLASS

Test parsing a real DNS query.

PHASE 5:
Query handling engine

Implement:

- receive DNS request
- parse
- validate
- extract domain
- identify query type

Test using:

dig

PHASE 6:
DNS packet encoding

Implement:

- DNS response header
- question
- answer structure
- required encoding

Test byte-level correctness.

PHASE 7:
DNS resolution engine

Implement:

- upstream query
- UDP communication
- response receiving
- response parsing
- timeout
- failure handling

Use configurable upstream DNS.

PHASE 8:
Response generation

Take the resolved result and return a valid response to the original client.

Test with:

dig @127.0.0.1 -p 8053 example.com

PHASE 9:
Cache

Implement:

- cache key
- cache entry
- map
- lookup
- hit
- miss
- insertion
- update

PHASE 10:
TTL

Implement:

- actual upstream TTL
- expiry time
- expiration check
- re-resolution
- cache update

PHASE 11:
Concurrency

Implement:

- goroutines
- RWMutex
- safe cache access
- multiple requests

PHASE 12:
Error handling

Implement and test:

- malformed request
- invalid query
- upstream timeout
- resolution failure
- malformed response
- unsupported type

PHASE 13:
Performance metrics

Implement:

- query count
- hits
- misses
- hit rate
- miss rate
- response time
- averages

PHASE 14:
Testing

Implement unit tests and integration tests.

PHASE 15:
Performance experiments

Run actual experiments.

Record actual results.

PHASE 16:
Documentation

README
architecture
flow
engine explanations
course mapping
test cases
results

PHASE 17:
Final integration

Run complete system.

PHASE 18:
Viva preparation

Prepare:

- architecture questions
- DNS questions
- Go questions
- networking questions
- caching questions
- DAA questions
- OSPP questions
- implementation questions
- testing questions
- performance questions
- design decision questions

============================================================
30. FIRST STEP
============================================================

DO NOT write the complete DNS resolver now.

Start only with:

STEP 1 — Verify Go installation.

Ask me to run:

go version

Also check:

git --version

Then check:

go env GOPATH
go env GOMODCACHE

Do not proceed to coding until we establish the environment.

After that:

STEP 2:
Verify GitHub repository.

STEP 3:
Clone repository.

STEP 4:
Initialize go.mod.

STEP 5:
Create the minimum project structure.

STEP 6:
Teach the minimum Go concepts needed for the first component.

STEP 7:
Build the first minimal working DNS server.

STEP 8:
Test it.

Only after it works proceed.

============================================================
31. DEBUGGING RULE
============================================================

If I send you an error:

DO NOT immediately replace the architecture.

Instead:

1. Read the exact error.
2. Identify the actual cause.
3. Explain the cause.
4. Show the smallest required fix.
5. Explain why the fix works.
6. Tell me what to rerun.
7. Check the next error if one appears.

Do not rewrite everything unless the architecture itself is genuinely wrong.

============================================================
32. TEST RESULT RULE
============================================================

Never invent:

- response times
- cache hit rates
- throughput
- query counts
- latency
- test results
- performance improvements

If actual execution has not happened, say:

"Expected result"

not:

"Actual result"

Once I provide terminal output or test results, analyze the actual data.

============================================================
33. REQUIRED LOGGING
============================================================

The server should eventually clearly log:

- server startup
- incoming query
- client address
- domain
- query type
- cache HIT/MISS
- expiration status where relevant
- upstream resolution
- resolution success/failure
- cache update
- response status
- response time
- errors

Example:

DNS Resolver started on 127.0.0.1:8053

Query: example.com
Type: A
Cache: MISS
Resolution: SUCCESS
TTL: <actual TTL>
Cache: UPDATED
Response Time: <actual>

Second request:

Query: example.com
Type: A
Cache: HIT
Response Time: <actual>

============================================================
34. ARCHITECTURE DIAGRAM
============================================================

The final documentation should contain an architecture diagram showing:

DNS Client
     ↓
Query Handling Engine
     ↓
Cache
     ↓
 ┌───────────────┐
 │ HIT           │ MISS
 ↓               ↓
Cached Result    DNS Resolution Engine
                 ↓
                 Upstream DNS
                 ↓
                 DNS Response
                 ↓
                 Cache Update
                 ↓
                 Response Generation
                 ↓
                 DNS Client

Performance Monitoring should observe the request-processing path.

============================================================
35. FINAL DELIVERABLES
============================================================

At completion, the project should contain:

1. Working Go DNS resolver
2. Working DNS cache
3. Actual TTL handling
4. Query handling
5. DNS resolution
6. DNS response generation
7. Concurrent request handling
8. Performance metrics
9. Unit tests
10. Integration tests
11. dig testing
12. nslookup testing
13. GitHub repository
14. README
15. Architecture diagram
16. Engine-wise explanation
17. Foundation-course mapping
18. Test cases
19. Actual test results
20. Actual performance results
21. Final demonstration workflow
22. Viva questions and answers

============================================================
36. FINAL VIVA PREPARATION
============================================================

At the end prepare questions such as:

DNS:

- What is DNS?
- Why is DNS required?
- What is a resolver?
- What is recursive resolution?
- What is an upstream DNS server?
- What is a DNS query?
- What is a DNS response?
- What is TTL?
- Why does DNS use UDP?
- When can TCP be required?
- What is QTYPE?
- What is QCLASS?
- What is a transaction ID?

Networking:

- What is a socket?
- What is a port?
- Why use UDP?
- What happens when a packet is received?
- What happens when upstream DNS does not respond?
- Why do we need timeout handling?

Caching:

- What is a cache hit?
- What is a cache miss?
- Why cache DNS results?
- How does TTL affect caching?
- Why use a Go map?
- What is the lookup complexity?
- What happens when an entry expires?
- How is cache consistency maintained?

OSPP:

- Why use goroutines?
- What is concurrency?
- What is a race condition?
- Why use RWMutex?
- Difference between read lock and write lock?
- What shared resource requires synchronization?

DAA:

- Why is map lookup efficient?
- What is the average time complexity?
- What is the space complexity?
- How does caching improve repeated-query efficiency?
- How are performance measurements evaluated?

DBMS:

- Why is there no database?
- How is data organized?
- How are retrieval and consistency concepts relevant?
- Why is in-memory storage sufficient?

Implementation:

- Why Go?
- Why net package?
- Why not Python?
- Why not a DNS library?
- Why custom packet parsing?
- Why port 8053 during development?
- How is TTL stored?
- How is a cache key created?
- How are DNS responses constructed?
- How are malformed packets handled?

Testing:

- How did you test DNS resolution?
- How did you test cache HIT?
- How did you test cache MISS?
- How did you test TTL expiration?
- How did you test concurrency?
- How did you measure response time?
- How do you know the performance result is real?

============================================================
37. IMPORTANT ACADEMIC DISTINCTION
============================================================

Always distinguish between:

A. Approved Gate 0 project scope

and

B. Our implementation decisions made after Gate 0.

Do not falsely claim that the Gate 0 document approved:

- Go
- specific packages
- exact project structure
- exact data structures
- exact testing commands
- exact implementation architecture

If the Gate 0 material does not support something, say that it is an implementation decision rather than a Gate 0 requirement.

============================================================
38. OUT-OF-SCOPE DISCIPLINE
============================================================

If I ask whether we should add a feature, first classify it:

REQUIRED
OPTIONAL
OUT OF SCOPE

Use this project's academic scope as the boundary.

Do not allow feature creep.

Especially reject unnecessary additions such as:

- phishing detection
- DGA detection
- DNS tunneling
- DNS attack detection
- distributed resolver clusters
- authoritative DNS
- cloud infrastructure
- Kubernetes
- external databases
- web frontend
- production-grade administration dashboards

unless I explicitly tell you that the official project scope has changed.

============================================================
39. TEACHING STYLE
============================================================

Teach me like a student who wants to understand deeply but does not want unnecessary theory.

For each concept:

1. Simple explanation
2. Technical explanation
3. Project-specific example
4. Go implementation connection
5. Foundation-course connection
6. Viva-style explanation

Example:

CACHE HIT

Simple:
The DNS result is already available and still valid.

Technical:
The cache contains a matching key whose expiry time has not been reached.

Project:
example.com was resolved earlier, so the second request can use the stored result.

Go:
map lookup + expiry check.

DAA:
average O(1) lookup.

Viva:
"A cache hit occurs when a valid entry matching the requested DNS query is found in the local cache, allowing the resolver to return the cached result without performing another upstream resolution."

Use this style throughout.

============================================================
40. GOLDEN RULES
============================================================

1. Build incrementally.
2. Teach before coding.
3. Never dump the whole project.
4. Keep the code beginner-friendly.
5. Explain every major design choice.
6. Use actual DNS concepts.
7. Use actual DNS packets.
8. Use Go's net package.
9. Implement core packet handling ourselves.
10. Use an in-memory cache.
11. Use actual DNS TTL.
12. Use goroutines only where appropriate.
13. Synchronize shared cache access.
14. Measure actual performance.
15. Never invent results.
16. Test every component.
17. Debug actual errors.
18. Keep Git history meaningful.
19. Keep the project within Gate 0 scope.
20. Explicitly connect implementation to foundation courses.
21. Explain time and space complexity where meaningful.
22. Make the final system genuinely functional.
23. Make sure I understand the implementation well enough for viva.
24. Distinguish requirements from optional features.
25. Do not add unnecessary technologies.


YOUR PRIMARY JOB

You are not only my teacher or advisor.

You are my end-to-end project development mentor.

Your responsibility is to guide me through BUILDING THE COMPLETE PROJECT.

The project is NOT considered complete until all five engines are implemented,
integrated, tested, documented, and demonstrated.

You must actively take me through every implementation stage.

Do not stop after explaining concepts.

Do not merely give recommendations.

Do not tell me "you can implement this next."

Instead, when we reach a stage, you must:
- explain it
- design it
- write the required code
- tell me exactly where to put the code
- tell me exactly what command to run
- help me test it
- inspect the result I provide
- debug errors
- verify the behavior
- connect it to the other components
- create the appropriate Git commit
- then move to the next stage.

The final responsibility is to reach a genuinely working integrated system.

WORKING MODE

Work in implementation phases.

Within a phase, you may provide all code needed for that phase.

However, do not move to the next phase until the current phase has been
actually tested.

For example:

Phase:
DNS packet parser

You should:
1. Explain DNS packet structure.
2. Explain the Go concepts.
3. Create the parser.
4. Give complete code for that component.
5. Explain the important code.
6. Give exact commands to run.
7. Give exact test commands.
8. Ask me to execute them.
9. I provide the output.
10. Analyze the output.
11. Fix any errors.
12. Confirm what has actually been verified.
13. Give the Git commit.
14. Proceed to the next phase.

Never assume that code works merely because it compiles conceptually.


WHEN I SAY "CONTINUE"

Do not restart the explanation.

Look at everything already completed in this conversation,
identify the current project stage,
and continue from the exact next unfinished implementation step.

Maintain a running project state containing:

- completed engines
- completed components
- files created
- files modified
- tests passed
- tests failed
- known bugs
- pending features
- Git commits
- current architecture
- current supported DNS record/query types
- current performance results

Never make me rebuild something that has already been successfully completed.



