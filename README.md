In this project, you will implement a DNS resolver capable of
performing basic name resolution. DNS is a distributed database system
that allows a collection of different authorities to agree on a
mapping from FQDNs to Resource Records. This system is part of the
core infrastructure of the modern Internet, and each of us typically
makes hundreds of DNS queries every day without noticing. Applications
of DNS include locating web and email servers, authenticating sent
emails, storing cryptographic certificates, and other metadata about
Internet services.

However, in this project we are only concerned with mapping FQDNs to
IPv4 addresses. This operation is used by your web browser when you
visit a website by its URL in order to determine the IP address of the
server to which it should request the web page/resource.

## Protocol

### RFC1035

Detailed descriptions of the DNS protocol, including binary message
formats are present in [RFC 1035](https://www.rfc-editor.org/info/rfc1035).
Your client should completely and faithfully implement message
encoding/decoding behaviors as described. We will only be implementing a
subset of the behaviors specified in the RFC, as described in the remainder
of this section.

### Transport

Full-featured DNS relies on UDP and TCP for transport. By default,
resolvers and nameservers communicate using UDP on port 53, and when
they observe a message being truncated, they will fallback to TCP.

Instead, you will *only implement UDP transport*, with no fallback to TCP.

### Error Handling

Because you will be communicating with remote servers with UDP
transport, you should not wait indefinitely for a reply to come from
any server. Instead, you should timeout after a short duration
(100-1000ms) and retry the query a limited number of times (2-5)
before contacting an alternative nameserver. If you exhaust all known
nameservers, you should exit with `EXIT_FAILURE`

### Types & Classes

DNS (especially modern DNS defined after RFC1035) supports an
extremely wide variety of record classes and types. The majority of
these classes and types are unnecessary for resolving IPv4 records,
and so are out of scope for this assignment. You will only be required
to support `CLASS=1 (IN)` and the following Resource Record TYPEs:

 1. `A` (type 1) an IPv4 host address
 2. `NS` (type 2) an authoritative name server
 3. `CNAME` (type 5) the canonical name for an alias

### Iterative Name Resolution

Full-featured DNS allows for the presence of recursive
nameservers. These are nameservers which (when requested with the RD
bit) perform subqueries to other nameservers on the resolver's
behalf. This allows for a resolver to make a single query and return
with meaningful information, and allows nameservers to cache
information for better performance. The resolution algorithm used in
DNS is described in detail in
[RFC 1034](https://www.rfc-editor.org/info/rfc1034).

Instead, you will implement an *iterative resolver*, which does not make
use of recursive queries. We expect you to implement the following algorithm
in your resolver, without the use of any recursive queries.

#### Iterative Resolution Algorithm

The host you are looking up is `target`, and your initial nameserver
(by IP address) is `bootstrap_ns_addr`. Note that you will need to
parse *all* resource records you find (including AAAA), even if you
don't print or use them. Print A, NS, and CNAME records as you parse
them, regardless of whether you use them or resolution succeeds.

 1. Begin with a query for root nameservers, using the given `bootstrap_ns_addr`
    
        QUERY(bootstrap_ns_addr, NS, ".")
    
 2. This returns a list of NS records, probably including glue records.
 3. set `current_ns` to one of the nameserver IP addresses returned by
    `bootstrap_ns_addr`
 4. You will then resolve `target` with the following process:
    1. `QUERY(current_ns, A, target)`
    2. Parse the resource records, printing all of the A, NS, and CNAME records
       you find.
    3. **if** the answers contain an A record for `target`, you are done
    4. **else if** the answers contain a CNAME record for `target`, change
       `target` to the data in the record
    5. **else if** the authority/nameserver records contain results:
       1. For each nameserver (the data of the record), find the corresponding
          A record in the additional records, and add the IP address to a
	  list of candidates for the next step.
       2. If there were no glue records provided, you will need to store
          the nameserver FQDNs instead, and perform queries for their A
	  records as needed. *These may be recursive queries.*
       3. Choose one nameserver from this list, and change `current_ns` to
          its IP address.
    6. **else if** the query failed to return a result, select another
       nameserver from the current step of the iteration (ie, another root
       server if this was the first step past the bootstrap server). If there
       are no more candidate nameservers at the step, exit with failure.
    7. Return to step 1 of this block (the A record query).

### Caching & Additional Records

The iterative resolution algorithm as specified makes several return
trips to the same nameserver, each time asking for different
information. For performance reasons, a nameserver will frequently
produce Additional Resource Records (`Additional RRs`). These are
records that are likely to be queried immediately after receiving the
results from the primary query.

When providing `Authority RRs`, nameservers will often (but not
always) serve an **A** record for each Authority RR. These are commonly
referred to as Glue Records. Line 9 in the above algorithm demonstrates a
logical query being executed that should instead be completed with the
results from glue records via a caching strategy. However, if that
caching strategy should fail, your resolver should query to fill the cache
miss. *This is the only query that is permitted to be recursive.*

## Implementation

The resolver will be a command line utility named `resolve`, which takes the
following arguments:

 1. `-a <String>` = The IP address of the initial nameserver. Represented
    in dotted quads as an ASCII string (e.g., 199.7.91.13). Must be specified.
 2. `-d <String>` = Domain name to resolve to an IP. This may not necessarily
    be fully-qualified (with or without the trailing period). Represented as
    an ASCII string. Must be specified.

An example usage is as follows:

    resolve -a 199.7.91.13 -d www.google.com

The client should perform iterative resolution of the A record
associated with the given domain name. The iterative resolution should
follow the procedure described in the provided algorithm.  As your
client is resolving the address, it may make multiple queries to
different nameservers. For each query you complete, you should print
out all of the non-question, supported RRs in the following format:

    <ns address>: <domain> <type> <data>

Where:

 1. **ns_address** is the nameserver which is serving you this RR, in
dotted quads
 2. **domain** is the NAME field from the RR
 3. **type** is the type field from the RR as a string (one of
    {**A**, **NS**, **CNAME**})
 4. **data** is the string representation of the resource data (dotted
    quads or FQDN)
 
Because you are implementing an iterative resolver, you will be making
multiple requests to different nameservers. This output format is not
order-sensitive, and you may print out records in whatever order is
most convenient. You may also print records which you later do not
use, so long as they were reasonable queries to make.

If you find an appropriate A record for the domain, you should print
it in the above format, and exit with `EXIT_SUCCESS`.

If your resolution does not find an appropriate A record, you should
exit without printing a above format, and exit with `EXIT_FAILURE`.

## Grading

Your project grade will depend on the parts of the project that you implement.

| **Grade** | **Parts Completed**                            |
| ---       | ---                                            |
| 25        | Iterative resolution without CNAME redirects   |
| 50        | Iterative resolution including CNAME redirects |

## Additional Requirements

 1. Your code must be submitted as a series of commits that are pushed to
    the origin/master branch of your Git repository. We consider your
    latest commit prior to the due date/time to represent your submission.
 2. **You may write your code in any language you choose.** If you choose to
    write it in C or C++, your code must be `-Wall` clean on gcc/g++ in
    the baseline image, otherwise your assignment will not be graded.
 3. You may not use any libraries that encode/decode DNS messages on your
    behalf, such as `scapy` or `dnspython`. If you want to use a library
    that you think might run afoul of this, ask one of the instructors to
    clarify whether it violates this requirement.
 4. You must provide a Makefile that is included along with the code that
    you commit. We will run `make` in the `assignment3` directory, which must
    produce a `resolve` executable also located in the `assignment3` directory.
 5. You must submit code that compiles in the baseline image, otherwise your
    assignment will not be graded.
 6. You are not allowed to copy code from any source.
