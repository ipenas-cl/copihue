# Chronos - Business Logic & Data Layer

**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Enfoque:** Lógica de negocio, validaciones, datos, integraciones

---

## 1. LÓGICA DE NEGOCIO

### 1.1 Business Rules (Reglas de Negocio)

**Template: BusinessRule**
```chronos
BusinessRule: order_minimum_amount
  Description: Orders must meet minimum purchase amount

  Applies To: Order
  Priority: high (blocking)

  Condition:
    order.total_amount >= minimum_purchase_amount

  When Violated:
    Return Error: "Order total ${order.total_amount} is below minimum ${minimum_purchase_amount}"

  Parameters:
    - minimum_purchase_amount: Money = 10.00 USD

  Active: always
  Audit: log all violations

BusinessRule: customer_credit_limit
  Description: Customer cannot exceed credit limit

  Applies To: Order
  Priority: critical (blocking)

  Condition:
    customer.current_balance + order.total_amount <= customer.credit_limit

  When Violated:
    Return Error: "Order would exceed credit limit"
    Notify: credit_department

  Dynamic Parameters:
    - customer.credit_limit (from customer profile)
    - customer.current_balance (from accounting system)

  Override: requires manager approval

BusinessRule: business_hours_only
  Description: Orders accepted only during business hours

  Applies To: Order
  Priority: medium

  Condition:
    current_time.is_between(9:00, 17:00) AND
    current_day.is_weekday()

  When Violated:
    Return Warning: "Order placed outside business hours, will process next business day"
    Action: queue_for_next_business_day(order)

  Exceptions:
    - Premium customers: allowed 24/7
    - Emergency orders: requires special flag
```

**Compilador genera:**
- Validation functions
- Error types
- Audit logging
- Rule engine runtime

---

### 1.2 Workflows (Flujos de Trabajo)

**Template: Workflow**
```chronos
Workflow: order_fulfillment
  Description: Complete order fulfillment process

  Trigger: OrderCreated event

  Steps:
    Step 1: Validate Order
      Action: validate_business_rules(order)
      On Success: → Step 2
      On Failure: → Cancel Order
      Timeout: 5 seconds

    Step 2: Check Inventory
      Action: inventory_service.check_availability(order.items)
      On Success: → Step 3
      On Failure: → Backorder Flow
      Timeout: 10 seconds
      Retry: 3 times with exponential backoff

    Step 3: Reserve Stock
      Action: inventory_service.reserve(order.items)
      Transaction: required
      On Success: → Step 4
      On Failure: → Cancel Order (rollback)
      Compensation: release_reservation(order.items)

    Step 4: Process Payment
      Action: payment_service.charge(order.payment_method, order.total)
      Transaction: required
      On Success: → Step 5
      On Failure: → Payment Failed Flow
      Idempotent: yes (idempotency_key = order.id)
      Timeout: 30 seconds

    Step 5: Create Shipment
      Action: shipping_service.create_shipment(order)
      On Success: → Step 6
      On Failure: → Refund Payment (compensation)

    Step 6: Send Confirmation
      Action: notification_service.send_email(order.customer, "Order Confirmed")
      On Failure: log error (non-critical)

    Step 7: Complete
      Action: mark_order_as_confirmed(order)
      Emit Event: OrderConfirmed

  Compensation Flow:
    # Executed if workflow fails after partial completion
    - Refund payment (if charged)
    - Release inventory (if reserved)
    - Notify customer of failure
    - Create support ticket

  Timeout: 2 minutes (entire workflow)

  Monitoring:
    - Track step completion times
    - Alert if timeout approaching
    - Metrics: success rate, avg duration

  State Persistence:
    - Save after each step
    - Resumable if system crashes
    - Saga pattern for distributed transactions
```

**Template: Saga (Distributed Transaction)**
```chronos
Saga: book_trip
  Description: Book flight + hotel + car (distributed transaction)

  Pattern: orchestration  # or: choreography

  Participants:
    - flight_service
    - hotel_service
    - car_rental_service
    - payment_service

  Steps:
    Step 1: Book Flight
      Service: flight_service
      Action: book_flight(trip.flight_details)
      Compensation: cancel_flight(booking_id)
      Timeout: 30 seconds

    Step 2: Book Hotel
      Service: hotel_service
      Action: book_hotel(trip.hotel_details)
      Compensation: cancel_hotel(booking_id)
      Timeout: 30 seconds

    Step 3: Book Car
      Service: car_rental_service
      Action: book_car(trip.car_details)
      Compensation: cancel_car(booking_id)
      Timeout: 30 seconds

    Step 4: Charge Customer
      Service: payment_service
      Action: charge(customer, total_amount)
      Compensation: refund(transaction_id)
      Timeout: 60 seconds

  On Failure:
    - Execute compensations in reverse order
    - Log failure reason
    - Notify customer
    - Create support case

  Consistency: eventual

  Isolation: semantic locks (e.g., hold reservations)

  Durability: all steps logged to event store
```

---

### 1.3 State Machines

**Template: StateMachine**
```chronos
StateMachine: order_lifecycle
  Description: Order state management

  States:
    - Draft (initial)
    - Submitted
    - Validated
    - PaymentPending
    - PaymentConfirmed
    - InFulfillment
    - Shipped
    - Delivered (final)
    - Cancelled (final)
    - Refunded (final)

  Transitions:
    From Draft:
      - To Submitted:
          Event: customer_submits_order
          Condition: order.items.count > 0
          Action: validate_order()

    From Submitted:
      - To Validated:
          Event: validation_passed
          Action: calculate_totals()

      - To Cancelled:
          Event: validation_failed
          Action: notify_customer(errors)

    From Validated:
      - To PaymentPending:
          Event: initiate_payment
          Action: create_payment_intent()

    From PaymentPending:
      - To PaymentConfirmed:
          Event: payment_succeeded
          Action: reserve_inventory()

      - To Cancelled:
          Event: payment_failed
          Action: notify_customer()

    From PaymentConfirmed:
      - To InFulfillment:
          Event: start_fulfillment
          Action: create_pick_list()

      - To Refunded:
          Event: customer_cancels
          Condition: cancellation_allowed()
          Action: process_refund()

    From InFulfillment:
      - To Shipped:
          Event: shipment_created
          Action: send_tracking_info()

    From Shipped:
      - To Delivered:
          Event: delivery_confirmed
          Action: close_order()

      - To Refunded:
          Event: return_initiated
          Action: process_return()

  Invariants:
    - Cannot transition from final state
    - All transitions must be logged
    - State changes must be atomic
    - Events must be idempotent

  Persistence:
    - Store current state
    - Store transition history
    - Enable event sourcing

  Visualization:
    - Generate state diagram
    - Export as DOT/Graphviz
```

---

### 1.4 Validations (Validaciones)

**Template: Validation**
```chronos
Validation: email_address
  Description: Validate email format and deliverability

  Field: email (String)

  Rules:
    Rule 1: Format Check
      Pattern: ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$
      Error: "Invalid email format"

    Rule 2: Length Check
      Min Length: 5
      Max Length: 254
      Error: "Email must be between 5 and 254 characters"

    Rule 3: Blacklist Check
      Condition: NOT email.domain IN blacklisted_domains
      Error: "Email domain not allowed"

    Rule 4: MX Record Check (optional)
      Action: dns_lookup_mx(email.domain)
      Timeout: 2 seconds
      Error: "Email domain has no mail server"
      Required: no (warning only)

  Sanitization:
    - Trim whitespace
    - Convert to lowercase

  Example Valid:
    - user@example.com
    - john.doe+tag@company.co.uk

  Example Invalid:
    - not-an-email
    - @missing-local.com
    - user@.com

Validation: credit_card
  Description: Validate credit card number

  Field: card_number (String)

  Rules:
    Rule 1: Format
      Pattern: digits only
      Action: remove spaces and dashes

    Rule 2: Length
      Condition: length IN [13, 14, 15, 16, 19]
      Error: "Invalid card number length"

    Rule 3: Luhn Check
      Algorithm: luhn_checksum(card_number) % 10 == 0
      Error: "Invalid card number (checksum failed)"

    Rule 4: Card Type Detection
      Patterns:
        - Visa: ^4
        - Mastercard: ^5[1-5]
        - Amex: ^3[47]
        - Discover: ^6(?:011|5)

  Security:
    - Never log full card number
    - Mask in display (show last 4 digits)
    - Encrypt at rest
    - PCI DSS compliant storage

Validation: business_logic_constraint
  Description: Complex multi-field validation

  Applies To: Order

  Rules:
    Rule 1: Discount Validity
      Condition:
        IF order.discount_code IS NOT NULL THEN
          - discount.is_active == true
          - discount.valid_from <= current_date <= discount.valid_until
          - order.total_before_discount >= discount.minimum_amount
          - customer.email NOT IN discount.excluded_customers
      Error: "Discount code invalid or not applicable"

    Rule 2: Stock Availability
      Condition:
        FOR EACH item IN order.items:
          inventory.available(item.sku) >= item.quantity
      Error: "Insufficient stock for items: ${out_of_stock_items}"

    Rule 3: Shipping Address
      Condition:
        IF order.requires_shipping THEN
          order.shipping_address IS NOT NULL AND
          order.shipping_address.is_valid()
      Error: "Valid shipping address required"

  Execution:
    - Run in transaction
    - All rules must pass
    - Short-circuit on first failure (optional)
```

---

## 2. PROCESAMIENTO DE DATOS

### 2.1 ETL (Extract, Transform, Load)

**Template: ETL Pipeline**
```chronos
ETLPipeline: customer_data_import
  Description: Import customer data from CSV to database

  Schedule: daily at 2:00 AM
  Timeout: 1 hour

  Extract:
    Source: CSV File
      Location: s3://data-bucket/customers/daily/customers_${date}.csv
      Format: CSV
      Delimiter: ","
      Headers: yes
      Encoding: UTF-8

    On File Missing:
      Action: log warning
      Continue: no

    On Read Error:
      Action: retry 3 times
      Then: fail pipeline

  Transform:
    Step 1: Parse Records
      Action: parse_csv_row(row) -> CustomerRecord
      On Error: log error, skip row

    Step 2: Validate
      Rules:
        - email is valid
        - phone is valid format
        - country code exists
      On Invalid: log to errors table, skip

    Step 3: Enrich
      Actions:
        - Geocode address (call geocoding API)
        - Lookup account status (query CRM)
        - Calculate customer tier

    Step 4: Deduplicate
      Key: email
      Strategy: keep most recent
      Action: merge with existing data

    Step 5: Normalize
      Actions:
        - Standardize phone format
        - Uppercase country codes
        - Trim all strings

  Load:
    Destination: PostgreSQL
      Table: customers
      Mode: upsert (ON CONFLICT UPDATE)
      Batch Size: 1000 records
      Transaction: yes

    On Duplicate Key:
      Action: update existing record
      Keep: newer data

    On Load Error:
      Action: rollback batch
      Retry: 3 times

  Monitoring:
    Metrics:
      - Records extracted
      - Records transformed
      - Records loaded
      - Records skipped
      - Errors count

    Alerts:
      - Error rate > 5%
      - Duration > 30 minutes
      - No file found

  Logging:
    - Log start/end times
    - Log each step completion
    - Log errors with context
    - Store in audit table

ETLPipeline: real_time_event_processing
  Description: Process user events stream

  Source: Kafka Topic
    Topic: user_events
    Consumer Group: analytics
    Offset: latest
    Parallelism: 8 partitions

  Transform:
    Step 1: Deserialize
      Format: JSON
      Schema: user_event_schema_v1

    Step 2: Filter
      Condition: event.type IN ['click', 'purchase', 'signup']

    Step 3: Enrich
      - Join with user profile (cache lookup)
      - Add session information
      - Add geolocation

    Step 4: Aggregate
      Windows:
        - 1 minute tumbling window
        - 5 minute sliding window (1 min slide)
      Metrics:
        - Count events by type
        - Sum purchase amounts
        - Unique users

  Load:
    Destinations:
      - PostgreSQL (aggregated metrics)
      - ClickHouse (raw events)
      - Redis (real-time counters)

    Batch: every 10 seconds or 1000 records

  Fault Tolerance:
    - Checkpointing: every 1 minute
    - State backend: RocksDB
    - Restart strategy: fixed-delay (3 attempts, 10s delay)
```

---

### 2.2 Data Transformations

**Template: Transformation**
```chronos
Transformation: normalize_address
  Description: Standardize address format

  Input: RawAddress
    Fields:
      - street: String
      - city: String
      - state: String
      - zip: String
      - country: String

  Output: NormalizedAddress
    Fields:
      - street_line1: String
      - street_line2: String (optional)
      - city: String
      - state_code: String (2 letters)
      - postal_code: String
      - country_code: String (ISO 3166-1 alpha-2)

  Rules:
    Step 1: Clean Input
      - Trim whitespace
      - Remove extra spaces
      - Uppercase state and country

    Step 2: Parse Street
      - Extract number and name
      - Identify apartment/suite
      - Split into line1 and line2

    Step 3: Standardize State
      - Map full name to code (e.g., "California" -> "CA")
      - Validate against known states

    Step 4: Normalize Postal Code
      - Format ZIP+4: xxxxx-xxxx
      - Validate format by country

    Step 5: Standardize Country
      - Map to ISO code
      - Validate against ISO 3166-1

  External Services:
    - USPS Address Validation API (US addresses)
    - Google Geocoding API (geocoding)

  Caching:
    - Cache normalized results (key: hash of input)
    - TTL: 30 days

  Error Handling:
    - Return original + error list if cannot normalize
    - Log unresolved addresses

Transformation: calculate_derived_metrics
  Description: Calculate business metrics from raw data

  Input: SalesRecord
    Fields:
      - sale_amount: Money
      - cost_amount: Money
      - quantity: Integer
      - sale_date: Date
      - product_category: String

  Output: SalesMetrics
    Fields:
      - revenue: Money
      - cost: Money
      - profit: Money
      - profit_margin: Percentage
      - units_sold: Integer
      - average_price: Money
      - fiscal_quarter: String
      - category: String

  Calculations:
    revenue:
      Formula: sale_amount

    cost:
      Formula: cost_amount

    profit:
      Formula: revenue - cost

    profit_margin:
      Formula: (profit / revenue) * 100
      Format: 2 decimal places
      Handle: division by zero -> 0%

    average_price:
      Formula: revenue / quantity
      Handle: quantity = 0 -> NULL

    fiscal_quarter:
      Formula: fiscal_quarter(sale_date, fiscal_year_start = "April")
      Example: "Q2 FY2024"

  Validation:
    - revenue >= 0
    - cost >= 0
    - profit_margin >= -100 AND <= 100

Transformation: data_masking
  Description: Mask sensitive data for non-production

  Input: Customer
  Output: MaskedCustomer

  Rules:
    email:
      Strategy: preserve_domain
      Example: "john.doe@example.com" -> "xxx****@example.com"

    phone:
      Strategy: mask_middle
      Example: "+1-555-1234" -> "+1-***-1234"

    credit_card:
      Strategy: last_four_only
      Example: "4111111111111111" -> "************1111"

    ssn:
      Strategy: full_mask
      Example: "123-45-6789" -> "***-**-****"

    address:
      Strategy: partial
      Keep: city, state, country
      Mask: street, postal_code

    name:
      Strategy: fake
      Use: faker library with consistent seed (same input -> same fake output)

  Environments:
    - Production: no masking
    - Staging: full masking
    - Development: full masking
    - Testing: full masking
```

---

## 3. CONEXIÓN A FUENTES DE DATOS

### 3.1 Database Connections

**Template: Database**
```chronos
Database: primary_db
  Description: Main PostgreSQL database

  Type: PostgreSQL
  Version: 15.x

  Connection:
    Host: db.example.com
    Port: 5432
    Database: production
    Username: app_user
    Password: ${env.DB_PASSWORD}  # From environment
    SSL: required
    SSL Mode: verify-full

  Pool:
    Min Connections: 5
    Max Connections: 20
    Idle Timeout: 10 minutes
    Max Lifetime: 30 minutes
    Connection Timeout: 5 seconds

  Performance:
    - Statement Timeout: 30 seconds
    - Idle in Transaction Timeout: 60 seconds
    - Prepared Statements: enabled
    - Query Logging: slow queries > 1s

  Health Check:
    Query: "SELECT 1"
    Interval: 30 seconds
    Timeout: 5 seconds

  Migrations:
    Tool: integrated
    Location: ./migrations/
    Auto Apply: no (production)
    Versioning: sequential numbers

Database: analytics_db
  Type: ClickHouse

  Connection:
    Hosts: [clickhouse1:9000, clickhouse2:9000, clickhouse3:9000]
    Database: analytics
    User: readonly_user
    Compression: lz4

  Pool:
    Max Connections: 10

  Query Options:
    - Max Execution Time: 60 seconds
    - Read Only: yes
    - Distributed: yes

Database: cache_db
  Type: Redis

  Connection:
    Nodes:
      - redis1:6379
      - redis2:6379
      - redis3:6379
    Mode: cluster
    Password: ${env.REDIS_PASSWORD}

  Pool:
    Max Connections: 50

  Options:
    - Default TTL: 1 hour
    - Max Memory Policy: allkeys-lru
    - Eviction: yes
```

**Template: Query**
```chronos
Query: get_customer_orders
  Description: Fetch customer orders with details

  Database: primary_db

  SQL: |
    SELECT
      o.order_id,
      o.order_date,
      o.status,
      o.total_amount,
      array_agg(
        json_build_object(
          'item_id', oi.item_id,
          'product_name', p.name,
          'quantity', oi.quantity,
          'price', oi.price
        )
      ) as items
    FROM orders o
    JOIN order_items oi ON o.order_id = oi.order_id
    JOIN products p ON oi.product_id = p.product_id
    WHERE o.customer_id = $1
      AND o.order_date >= $2
    GROUP BY o.order_id
    ORDER BY o.order_date DESC
    LIMIT $3

  Parameters:
    - customer_id: Integer (required)
    - start_date: Date (required)
    - limit: Integer (default: 10, max: 100)

  Returns: List<Order>
    Fields:
      - order_id: Integer
      - order_date: Date
      - status: OrderStatus
      - total_amount: Money
      - items: List<OrderItem>

  Caching:
    - Enabled: yes
    - TTL: 5 minutes
    - Key: "orders:${customer_id}:${start_date}:${limit}"
    - Invalidate On: order_created, order_updated

  Performance:
    - Explain Plan: analyzed at startup
    - Expected Rows: ~50
    - Expected Time: <100ms
    - Alert If: >500ms

Query: insert_order
  Description: Create new order

  Database: primary_db

  SQL: |
    INSERT INTO orders (
      customer_id,
      order_date,
      status,
      total_amount,
      shipping_address_id
    ) VALUES ($1, $2, $3, $4, $5)
    RETURNING order_id, created_at

  Parameters:
    - customer_id: Integer
    - order_date: Date
    - status: OrderStatus
    - total_amount: Money
    - shipping_address_id: Integer (nullable)

  Returns: OrderResult
    - order_id: Integer
    - created_at: Timestamp

  Transaction: required

  Validation:
    - customer exists (foreign key)
    - total_amount > 0
    - shipping_address belongs to customer

  Side Effects:
    - Emit OrderCreated event
    - Update customer last_order_date
    - Invalidate cache for customer orders
```

---

### 3.2 API Connections

**Template: RestAPI**
```chronos
RestAPI: payment_gateway
  Description: Stripe payment processing

  Base URL: https://api.stripe.com/v1

  Authentication:
    Type: Bearer Token
    Token: ${env.STRIPE_SECRET_KEY}

  Headers:
    - Stripe-Version: 2023-10-16
    - Idempotency-Key: ${request_id}  # For POST/PUT

  Timeout:
    - Connect: 5 seconds
    - Read: 30 seconds

  Retry:
    - Max Attempts: 3
    - Strategy: exponential backoff
    - Retry On: [500, 502, 503, 504]

  Endpoints:
    Create Payment Intent:
      Method: POST
      Path: /payment_intents

      Request:
        Body: JSON
          Fields:
            - amount: Integer (cents)
            - currency: String (ISO 4217)
            - customer: String (customer_id)
            - payment_method: String
            - confirm: Boolean

      Response:
        Status: 200
        Body: PaymentIntent
          Fields:
            - id: String
            - status: String
            - amount: Integer
            - currency: String
            - client_secret: String

      Errors:
        - 400: Invalid parameters
        - 402: Card declined
        - 500: Server error

      Rate Limit: 100 req/second

    Retrieve Payment Intent:
      Method: GET
      Path: /payment_intents/{id}

      Parameters:
        - id: String (path parameter)

      Response:
        Status: 200
        Body: PaymentIntent

      Caching: yes (TTL: 1 minute)

RestAPI: geocoding_service
  Description: Google Geocoding API

  Base URL: https://maps.googleapis.com/maps/api

  Authentication:
    Type: API Key
    Parameter: key
    Value: ${env.GOOGLE_MAPS_API_KEY}

  Endpoints:
    Geocode Address:
      Method: GET
      Path: /geocode/json

      Parameters:
        - address: String (URL encoded)
        - key: String (API key)

      Response:
        Body: GeocodingResult
          Fields:
            - results: List<Result>
            - status: String

      Caching:
        - Enabled: yes
        - TTL: 30 days
        - Key: "geocode:${hash(address)}"

      Rate Limit: 50 req/second

      Cost: $0.005 per request
```

**Template: GraphQL API**
```chronos
GraphQLAPI: github_api
  Description: GitHub GraphQL API

  Endpoint: https://api.github.com/graphql

  Authentication:
    Type: Bearer Token
    Token: ${env.GITHUB_TOKEN}

  Query: get_repository
    Description: Fetch repository information

    GraphQL: |
      query GetRepository($owner: String!, $name: String!) {
        repository(owner: $owner, name: $name) {
          name
          description
          stargazerCount
          forkCount
          issues(first: 10, states: OPEN) {
            nodes {
              title
              createdAt
              author {
                login
              }
            }
          }
        }
      }

    Variables:
      - owner: String
      - name: String

    Returns: Repository

  Mutation: create_issue
    Description: Create new issue

    GraphQL: |
      mutation CreateIssue($repositoryId: ID!, $title: String!, $body: String) {
        createIssue(input: {
          repositoryId: $repositoryId,
          title: $title,
          body: $body
        }) {
          issue {
            id
            number
            title
            url
          }
        }
      }

    Variables:
      - repositoryId: ID
      - title: String
      - body: String (optional)

    Returns: Issue
```

---

### 3.3 Message Queues

**Template: MessageQueue**
```chronos
MessageQueue: order_events
  Description: Order processing event queue

  Type: RabbitMQ

  Connection:
    Hosts: [rabbit1:5672, rabbit2:5672, rabbit3:5672]
    Virtual Host: /production
    Username: app_user
    Password: ${env.RABBITMQ_PASSWORD}
    Heartbeat: 60 seconds

  Exchange:
    Name: orders
    Type: topic
    Durable: yes
    Auto Delete: no

  Queue:
    Name: order_processing
    Durable: yes
    Exclusive: no
    Auto Delete: no
    Max Length: 10000
    Message TTL: 1 day

  Bindings:
    - Routing Key: order.created
    - Routing Key: order.updated
    - Routing Key: order.cancelled

  Consumer:
    Prefetch Count: 10
    Auto Ack: no (manual ack)
    Consumer Tag: order_processor_1

    On Message:
      - Parse message
      - Process order
      - Acknowledge (or reject)

    Error Handling:
      - Dead Letter Exchange: orders_dlx
      - Dead Letter Queue: orders_failed
      - Max Retries: 3
      - Retry Delay: exponential backoff

  Publisher:
    Confirm Mode: yes (publisher confirms)
    Mandatory: yes (return unroutable)
    Persistent: yes (messages survive restart)

MessageQueue: real_time_notifications
  Type: Apache Kafka

  Connection:
    Brokers: [kafka1:9092, kafka2:9092, kafka3:9092]
    Security: SASL_SSL
    Mechanism: SCRAM-SHA-512

  Topic:
    Name: notifications
    Partitions: 12
    Replication Factor: 3
    Retention: 7 days
    Compression: snappy

  Producer:
    Acks: all (wait for all replicas)
    Retries: 5
    Idempotence: yes
    Batch Size: 16 KB
    Linger: 10 ms

  Consumer:
    Group ID: notification_handlers
    Auto Offset Reset: earliest
    Enable Auto Commit: no (manual commit)
    Max Poll Records: 500

    On Message:
      - Deserialize (Avro schema)
      - Route by notification type
      - Send via appropriate channel
      - Commit offset

  Schema Registry:
    URL: http://schema-registry:8081
    Subject: notifications-value
    Compatibility: BACKWARD
```

---

### 3.4 File Systems

**Template: FileStorage**
```chronos
FileStorage: upload_storage
  Description: User file uploads

  Type: S3

  Configuration:
    Bucket: user-uploads-prod
    Region: us-east-1
    Access Key: ${env.AWS_ACCESS_KEY}
    Secret Key: ${env.AWS_SECRET_KEY}

  Paths:
    - /avatars/{user_id}/{filename}
    - /documents/{user_id}/{year}/{month}/{filename}
    - /temp/{upload_id}/{filename}

  Upload:
    Max File Size: 10 MB
    Allowed Types: [image/jpeg, image/png, application/pdf]
    Virus Scan: required (ClamAV)
    Encryption: AES-256 (server-side)

    Pre-signed URLs:
      - Expiration: 15 minutes
      - Method: PUT
      - Conditions:
          - Max size: 10 MB
          - Content type: allowed types only

  Download:
    Pre-signed URLs:
      - Expiration: 1 hour
      - Method: GET
      - Cache-Control: public, max-age=3600

  Lifecycle:
    - Temp files: delete after 24 hours
    - Old avatars: archive to Glacier after 1 year
    - Deleted files: move to versioned bucket

  Security:
    - Public Access: blocked
    - CORS: configured for web uploads
    - Logging: all access logged to separate bucket

FileStorage: local_cache
  Type: Local Filesystem

  Configuration:
    Base Path: /var/cache/app
    Max Size: 10 GB
    Permissions: 0755

  Paths:
    - /downloads/{file_id}
    - /thumbnails/{image_id}_{size}.jpg

  Cleanup:
    - Strategy: LRU (Least Recently Used)
    - Check Interval: 1 hour
    - Target Size: 8 GB (80% of max)
```

---

## 4. TRANSACCIONES Y CONSISTENCIA

### 4.1 Database Transactions

**Template: Transaction**
```chronos
Transaction: transfer_funds
  Description: Transfer money between accounts

  Isolation Level: SERIALIZABLE

  Steps:
    Step 1: Lock Accounts
      Query: |
        SELECT balance FROM accounts
        WHERE account_id IN ($1, $2)
        FOR UPDATE

      Validation:
        - Both accounts exist
        - Both accounts not frozen

    Step 2: Check Balance
      Condition: from_account.balance >= amount

      On Failure:
        Rollback: yes
        Error: "Insufficient funds"

    Step 3: Debit Source
      Query: |
        UPDATE accounts
        SET balance = balance - $1,
            updated_at = NOW()
        WHERE account_id = $2

    Step 4: Credit Destination
      Query: |
        UPDATE accounts
        SET balance = balance + $1,
            updated_at = NOW()
        WHERE account_id = $2

    Step 5: Record Transaction
      Query: |
        INSERT INTO transactions (
          from_account_id,
          to_account_id,
          amount,
          transaction_date
        ) VALUES ($1, $2, $3, NOW())

  Commit: if all steps succeed

  Rollback: on any failure

  Retry: 3 times on deadlock

  Timeout: 5 seconds

  Audit:
    - Log all steps
    - Log commit/rollback
    - Store in audit trail
```

---

### 4.2 Distributed Transactions

**Template: TwoPhaseCommit**
```chronos
TwoPhaseCommit: distributed_order
  Description: Create order across multiple databases

  Coordinator: order_service

  Participants:
    - inventory_db: reserve stock
    - payment_db: hold payment
    - shipping_db: create shipment

  Phase 1: Prepare
    For Each Participant:
      - Send prepare request
      - Wait for vote (yes/no)
      - Timeout: 10 seconds

    If All Vote Yes:
      → Phase 2: Commit
    Else:
      → Phase 2: Abort

  Phase 2: Commit or Abort
    For Each Participant:
      - Send commit (or abort) request
      - Wait for acknowledgment
      - Retry if needed

  Recovery:
    - Log all prepare/commit decisions
    - Re-send on participant failure
    - Timeout: 30 seconds total

  Failure Modes:
    - Participant timeout: abort
    - Coordinator crash: recovery from log
    - Network partition: manual intervention
```

---

## 5. INTEGRACIONES Y EVENTOS

### 5.1 Webhooks

**Template: Webhook**
```chronos
Webhook: stripe_payment_events
  Description: Receive Stripe payment events

  Endpoint: POST /webhooks/stripe

  Authentication:
    Type: HMAC Signature
    Header: Stripe-Signature
    Secret: ${env.STRIPE_WEBHOOK_SECRET}
    Algorithm: SHA256

  Events:
    - payment_intent.succeeded
    - payment_intent.payment_failed
    - charge.refunded
    - customer.subscription.deleted

  Handler:
    On payment_intent.succeeded:
      - Parse event data
      - Find order by payment_intent_id
      - Update order status to "paid"
      - Trigger fulfillment workflow
      - Send confirmation email

    On payment_intent.payment_failed:
      - Find order
      - Update status to "payment_failed"
      - Notify customer
      - Create support ticket

  Idempotency:
    - Check event ID (Stripe guarantees uniqueness)
    - Store processed event IDs
    - Skip if already processed

  Retry:
    - Stripe retries failed webhooks
    - Exponential backoff
    - Up to 3 days

  Response:
    - Success: 200 OK (within 5 seconds)
    - Failure: 500 (triggers retry)

Webhook: send_outgoing_webhook
  Description: Send webhooks to customer systems

  Trigger: OrderStatusChanged event

  Configuration:
    URL: customer.webhook_url (from customer settings)
    Method: POST
    Headers:
      - Content-Type: application/json
      - X-Webhook-Signature: ${hmac_signature}

  Payload:
    Format: JSON
    Fields:
      - event_type: String
      - event_id: UUID
      - timestamp: ISO8601
      - data: Object (event-specific)

  Delivery:
    - Timeout: 10 seconds
    - Retry: 3 times (with backoff)
    - Success: 2xx status code
    - Store delivery status

  Failure Handling:
    - Log failed deliveries
    - Dead letter queue for manual retry
    - Alert customer admin
```

---

### 5.2 Event Sourcing

**Template: EventStore**
```chronos
EventStore: order_events
  Description: Event-sourced order aggregate

  Stream: orders-{order_id}

  Events:
    OrderCreated:
      Fields:
        - order_id: UUID
        - customer_id: UUID
        - items: List<OrderItem>
        - total_amount: Money
        - created_at: Timestamp

    OrderItemAdded:
      Fields:
        - order_id: UUID
        - item: OrderItem

    OrderPaid:
      Fields:
        - order_id: UUID
        - payment_id: UUID
        - amount: Money
        - paid_at: Timestamp

    OrderShipped:
      Fields:
        - order_id: UUID
        - tracking_number: String
        - carrier: String
        - shipped_at: Timestamp

    OrderCancelled:
      Fields:
        - order_id: UUID
        - reason: String
        - cancelled_at: Timestamp

  Aggregate: Order
    State:
      - order_id
      - status
      - items
      - total_amount
      - payment_status
      - shipping_status

    Replay: from event stream
      - Start with empty state
      - Apply each event in order
      - Rebuild current state

  Projections:
    ReadModel: order_summary
      Table: order_summaries
      Update On: all order events
      Fields:
        - order_id
        - customer_id
        - status
        - total_amount
        - item_count
        - created_at
        - updated_at

    ReadModel: customer_orders
      Table: customer_order_list
      Update On: OrderCreated, OrderStatusChanged
      Indexed By: customer_id

  Snapshots:
    - Every 100 events
    - Store current aggregate state
    - Replay from last snapshot
```

---

## CONCLUSIÓN

Con estos templates, Chronos puede manejar:

✅ **Lógica de Negocio:**
- Business rules
- Workflows complejos
- State machines
- Validaciones sofisticadas

✅ **Procesamiento de Datos:**
- ETL pipelines
- Real-time streaming
- Data transformations
- Agregaciones

✅ **Conexiones:**
- Databases (SQL, NoSQL)
- REST APIs
- GraphQL
- Message queues
- File storage

✅ **Transacciones:**
- ACID local
- Distributed (2PC, Saga)
- Event sourcing

✅ **Integraciones:**
- Webhooks (in/out)
- Events
- External services

**Un lenguaje COMPLETO para aplicaciones empresariales reales.**

---

**¿Falta algo más?**
