// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package cmd

import (
	"fmt"

	"github.com/fatih/color"
	"github.com/spf13/cobra"
	"github.com/vespa-engine/vespa/client/go/internal/vespa"
)

func newDestroyCmd(cli *CLI) *cobra.Command {
	force := false
	targetFlags := NewTargetFlagsWithCLI(cli)
	cmd := &cobra.Command{
		Use:   "destroy",
		Short: "Remove a deployed Vespa application and its data",
		Long: `Remove a deployed Vespa application and its data.

This command removes the currently deployed application and permanently
deletes its data.

When run interactively, the command will prompt for confirmation before
removing the application. When run non-interactively, the command will refuse
to remove the application unless the --force option is given.

This command can only be used to remove non-production deployments, in Vespa
Cloud. See https://docs.vespa.ai/en/cloud/deleting-applications.html for how to remove
production deployments.

For other systems, destroy the application by removing the
containers in use by the application. For example:
https://github.com/vespa-engine/sample-apps/tree/master/examples/operations/multinode-HA#clean-up-after-testing`,
		Example: `$ vespa destroy
$ vespa destroy -a mytenant.myapp.myinstance
$ vespa destroy --force`,
		DisableAutoGenTag: true,
		SilenceUsage:      true,
		RunE: func(cmd *cobra.Command, args []string) error {
			target, err := targetFlags.GetTarget(cloudTargetOnly)
			if err != nil {
				return err
			}
			description := target.Deployment().String()
			env := target.Deployment().Zone.Environment
			if env != "dev" && env != "perf" {
				return errHint(fmt.Errorf("cannot remove production %s", description), "See https://docs.vespa.ai/en/cloud/deleting-applications.html")
			}
			ok := force
			if !ok {
				cli.printWarning(fmt.Sprintf("This operation will irrecoverably remove the %s and all of its data", color.RedString(description)))
				ok, _ = cli.confirm("Proceed with removal?", false)
			}
			if ok {
				err := vespa.Deactivate(vespa.DeploymentOptions{Target: target})
				if err == nil {
					cli.printSuccess(fmt.Sprintf("Removed %s", description))
				}
				return err
			}
			return fmt.Errorf("refusing to remove %s without confirmation", description)
		},
	}
	cmd.PersistentFlags().BoolVar(&force, "force", false, "Disable confirmation (default false)")
	targetFlags.AddFlags(cmd)
	return cmd
}
