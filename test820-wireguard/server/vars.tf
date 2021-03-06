variable "admin_user" {
  default = "vultr"
}

variable "admin_pubkeys" {
  type    = "list"
  default = []
}

resource "random_id" "subdomain_id" {
  byte_length = 4
}

locals {
  hostname = "test-${random_id.subdomain_id.hex}.example.com"
}

output "hostname" {
  value = "${local.hostname}"
}
